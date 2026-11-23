#include "modelo/Drone.hpp"
#include "modelo/Telemetria.hpp"
#include "modelo/Veiculo.hpp"

#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/action/action.hpp>
#include <mavsdk/plugins/mavlink_direct/mavlink_direct.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <utility>

namespace gcs {
namespace {

std::string para_maiusculas(std::string texto) {
  std::transform(texto.begin(), texto.end(), texto.begin(),
                 [](unsigned char letra) {
                   return static_cast<char>(std::toupper(letra));
                 });
  return texto;
}

/// O MAVSDK imprime os enums com operator<<; é a forma mais simples de
/// transformar um FlightMode em texto.
template <typename T> std::string para_texto(const T& valor) {
  std::ostringstream fluxo;
  fluxo << valor;
  return fluxo.str();
}

void acao_sucesso(const mavsdk::Action::Result res) {
  if (res != mavsdk::Action::Result::Success)
    throw ErroDeVeiculo("Erro na ação: " + para_texto(res));
}

} // namespace

std::string normalizar_endereco(const std::string& endereco) {
  if (endereco.find("://") != std::string::npos) {
    return endereco; // já está no formato de URL
  }
  const std::size_t separador = endereco.find(':');
  if (separador == std::string::npos) {
    return endereco;
  }
  std::string esquema = endereco.substr(0, separador);
  const std::string resto = endereco.substr(separador + 1);
  // "tcp" é ambíguo para o MAVSDK; para falar com o SITL queremos "tcpout".
  if (esquema == "tcp") {
    esquema = "tcpout";
  }
  return esquema + "://" + resto;
}

/// Tudo que depende do MAVSDK vive aqui dentro.
class Drone::Impl {
public:
  Impl()
      : nucleo(mavsdk::Mavsdk::Configuration{
            mavsdk::ComponentType::GroundStation}) {}

  mavsdk::Mavsdk nucleo;
  std::shared_ptr<mavsdk::System> sistema;
  std::unique_ptr<mavsdk::Telemetry> telemetria;
  std::unique_ptr<mavsdk::Action> acoes;
  std::unique_ptr<mavsdk::MavlinkDirect> direto;
};

Drone::Drone(std::string nome, std::string endereco)
    : Veiculo(std::move(nome)), impl_(std::make_unique<Impl>()),
      endereco_(std::move(endereco)) {}

Drone::~Drone() = default; // precisa estar no .cpp por causa do pimpl

void Drone::conectar() {
  const std::string url = normalizar_endereco(endereco_);
  std::cout << "[" << nome() << "] conectando em " << url << " ..."
            << std::endl;

  const mavsdk::ConnectionResult resultado =
      impl_->nucleo.add_any_connection(url);
  if (resultado != mavsdk::ConnectionResult::Success) {
    throw ErroDeVeiculo("não foi possível abrir " + url + ": " +
                        para_texto(resultado));
  }

  // O primeiro autopiloto que aparecer na rede MAVLink é o nosso drone.
  const auto sistema = impl_->nucleo.first_autopilot(TEMPO_LIMITE_S);
  if (!sistema) {
    throw ErroDeVeiculo("nenhum autopiloto respondeu em " + url +
                        ". O SITL do ArduPilot está rodando?");
  }

  impl_->sistema = sistema.value();
  impl_->telemetria = std::make_unique<mavsdk::Telemetry>(impl_->sistema);
  impl_->acoes = std::make_unique<mavsdk::Action>(impl_->sistema);
  impl_->direto = std::make_unique<mavsdk::MavlinkDirect>(impl_->sistema);
  std::cout << "[" << nome() << "] conectado." << std::endl;
}

void Drone::iniciar_telemetria() {
  if (!impl_->telemetria) {
    throw ErroDeVeiculo("iniciar_telemetria() antes de conectar()");
  }
  mavsdk::Telemetry& telemetria = *impl_->telemetria;

  // Cada assinatura é um callback chamado pela thread do MAVSDK. Tudo que
  // fazemos é traduzir o dado da biblioteca para o NOSSO modelo — assim o
  // resto do programa não depende do MAVSDK.
  telemetria.subscribe_flight_mode([this](mavsdk::Telemetry::FlightMode modo) {
    const std::string texto = para_texto(modo);
    atualizar([&texto](EstadoVeiculo& estado) { estado.modo_de_voo = texto; });
  });

  telemetria.subscribe_attitude_euler(
      [this](mavsdk::Telemetry::EulerAngle angulo) {
        atualizar([&angulo](EstadoVeiculo& estado) {
          estado.atitude.rolagem = angulo.roll_deg;
          estado.atitude.arfagem = angulo.pitch_deg;
          estado.atitude.guinada = angulo.yaw_deg;
        });
      });

  telemetria.subscribe_position([this](mavsdk::Telemetry::Position posicao) {
    atualizar([&posicao](EstadoVeiculo& estado) {
      estado.posicao.latitude = posicao.latitude_deg;
      estado.posicao.longitude = posicao.longitude_deg;
      estado.posicao.altitude_absoluta_m = posicao.absolute_altitude_m;
      estado.posicao.altitude_relativa_m = posicao.relative_altitude_m;
    });
  });

  telemetria.subscribe_battery([this](mavsdk::Telemetry::Battery bateria) {
    atualizar([&bateria](EstadoVeiculo& estado) {
      estado.bateria.tensao_v = bateria.voltage_v;
      // No MAVSDK 4.x remaining_percent já vem de 0 a 100 (nas versões
      // 1.x vinha de 0 a 1 — cuidado ao trocar de versão).
      estado.bateria.percentual = bateria.remaining_percent;
    });
  });

  telemetria.subscribe_armed([this](bool armado) {
    atualizar([armado](EstadoVeiculo& estado) { estado.armado = armado; });
  });

  telemetria.subscribe_in_air([this](bool em_voo) {
    atualizar([em_voo](EstadoVeiculo& estado) { estado.em_voo = em_voo; });
  });

  telemetria.subscribe_velocity_ned([this](mavsdk::Telemetry::VelocityNed v) {
    atualizar([&v](EstadoVeiculo& estado) {
      estado.velocidade = std::hypot(v.north_m_s, v.east_m_s);
      estado.taxa_subida_ms = -v.down_m_s; // NED: "down" positivo = descendo
    });
  });

  telemetria.subscribe_heading([this](mavsdk::Telemetry::Heading rumo) {
    atualizar([&rumo](EstadoVeiculo& estado) {
      estado.rumo_graus = rumo.heading_deg;
    });
  });

  telemetria.subscribe_gps_info([this](mavsdk::Telemetry::GpsInfo gps) {
    const std::string fix = para_texto(gps.fix_type);
    atualizar([&gps, &fix](EstadoVeiculo& estado) {
      estado.satelites = gps.num_satellites;
      estado.fix_gps = fix;
    });
  });

  telemetria.subscribe_rc_status([this](mavsdk::Telemetry::RcStatus rc) {
    atualizar([&rc](EstadoVeiculo& estado) {
      estado.sinal_radio_pct = std::isnan(rc.signal_strength_percent)
                                   ? 0.0
                                   : rc.signal_strength_percent;
    });
  });

  telemetria.subscribe_health([this](mavsdk::Telemetry::Health saude) {
    atualizar([&saude](EstadoVeiculo& estado) {
      estado.pronto_para_armar = saude.is_armable;
    });
  });

  telemetria.subscribe_home([this](mavsdk::Telemetry::HomePosition casa) {
    atualizar([&casa](EstadoVeiculo& estado) {
      estado.home.latitude = casa.latitude_deg;
      estado.home.longitude = casa.longitude_deg;
      estado.home.altitude_absoluta_m = casa.absolute_altitude_m;
    });
  });
}

void Drone::alterar_modo_de_voo(const std::string& modo) {
  if (!impl_->direto) {
    throw ErroDeVeiculo("alterar_modo_de_voo() antes de conectar()");
  }

  // Números dos modos do ArduCopter (o "custom_mode" do MAVLink).
  static const std::map<std::string, int> modos_ardupilot{
      {"STABILIZE", 0}, {"ACRO", 1},     {"ALT_HOLD", 2}, {"AUTO", 3},
      {"GUIDED", 4},    {"LOITER", 5},   {"RTL", 6},      {"CIRCLE", 7},
      {"LAND", 9},      {"POSHOLD", 16}, {"BRAKE", 17},   {"SMART_RTL", 21}};

  const std::string alvo = para_maiusculas(modo);
  const auto encontrado = modos_ardupilot.find(alvo);
  if (encontrado == modos_ardupilot.end()) {
    std::string disponiveis;
    for (const auto& par : modos_ardupilot) {
      disponiveis += (disponiveis.empty() ? "" : ", ") + par.first;
    }
    throw ErroDeVeiculo("modo '" + alvo +
                        "' desconhecido; disponíveis: " + disponiveis);
  }

  // MAV_CMD_DO_SET_MODE (176): param1 = 1 diz "use o número de modo do
  // autopiloto", param2 = esse número.
  const std::string sistema = std::to_string(impl_->sistema->get_system_id());
  mavsdk::MavlinkDirect::MavlinkMessage comando;
  comando.message_name = "COMMAND_LONG";
  comando.target_system_id = impl_->sistema->get_system_id();
  comando.target_component_id = 1;
  comando.fields_json = "{\"target_system\":" + sistema +
                        ",\"target_component\":1,\"command\":176"
                        ",\"confirmation\":0,\"param1\":1,\"param2\":" +
                        std::to_string(encontrado->second) +
                        ",\"param3\":0,\"param4\":0,\"param5\":0"
                        ",\"param6\":0,\"param7\":0}";

  const auto resultado = impl_->direto->send_message(comando);
  if (resultado != mavsdk::MavlinkDirect::Result::Success) {
    throw ErroDeVeiculo("falha ao enviar o modo " + alvo + ": " +
                        para_texto(resultado));
  }
}

// TODO (aluno): implemente aqui os demais comandos, sempre delegando para o
// plugin Action e convertendo o Result em exceção, como acima:

void Drone::armar() {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("armar() antes de conectar()");
  }
  std::cout << "armar" << std::endl;
  acao_sucesso(impl_->acoes->arm());
}

void Drone::desarmar() {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("desarmar() antes de conectar()");
  }
  std::cout << "desarmar" << std::endl;
  acao_sucesso(impl_->acoes->disarm());
}

void Drone::decolar(double altura_m) {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("decolar() antes de conectar()");
  }
  std::cout << "decolar" << std::endl;
  acao_sucesso(
      impl_->acoes->set_takeoff_altitude(static_cast<float>(altura_m)));
  acao_sucesso(impl_->acoes->takeoff());
}

void Drone::pousar() {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("pousar() antes de conectar()");
  }
  std::cout << "pousar" << std::endl;
  acao_sucesso(impl_->acoes->land());
}

void Drone::retornar_para_casa() {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("retornar_para_casa() antes de conectar()");
  }
  std::cout << "retornar para casa" << std::endl;
  acao_sucesso(impl_->acoes->return_to_launch());
}

void Drone::ir_para(Posicao pos) {
  if (!impl_->acoes) {
    throw ErroDeVeiculo("ir_para() antes de conectar()");
  }
  std::cout << "ir para" << std::endl;
  acao_sucesso(impl_->acoes->goto_location(pos.latitude, pos.longitude,
                                           pos.altitude_absoluta_m, NAN));
}

} // namespace gcs
