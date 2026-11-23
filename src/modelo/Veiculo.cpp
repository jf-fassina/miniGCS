#include "modelo/Veiculo.hpp"
#include "modelo/Telemetria.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <utility>

namespace gcs {

Veiculo::Veiculo(std::string nome) : nome_(std::move(nome)) {}

EstadoVeiculo Veiculo::estado() const {
  std::lock_guard<std::mutex> trava(mutex_);
  return estado_; // cópia
}

std::string Veiculo::modo_de_voo() const {
  std::lock_guard<std::mutex> trava(mutex_);
  return estado_.modo_de_voo;
}

void Veiculo::registrar_observador(Observador observador) {
  std::lock_guard<std::mutex> trava(mutex_);
  observadores_.push_back(std::move(observador));
}

void Veiculo::atualizar(
    const std::function<void(EstadoVeiculo&)>& modificacao) {
  EstadoVeiculo copia;
  std::vector<Observador> observadores;
  {
    std::lock_guard<std::mutex> trava(mutex_);
    modificacao(estado_);
    estado_.instante = std::chrono::steady_clock::now();
    copia = estado_;
    observadores = observadores_;
  }
  // Os observadores são chamados FORA da trava: se um deles pedir estado()
  // de volta, não travamos o programa.
  for (const Observador& observador : observadores) {
    observador(copia);
  }
}

std::ostream& operator<<(std::ostream& fluxo, const Veiculo& veiculo) {
  return fluxo << veiculo.nome() << ": " << veiculo.estado().resumo();
}

// ---------------------------------------------------------------------------
// VeiculoSimulado
// ---------------------------------------------------------------------------
VeiculoSimulado::VeiculoSimulado(std::string nome) : Veiculo(std::move(nome)) {}

VeiculoSimulado::~VeiculoSimulado() {
  parar_ = true;
  if (thread_.joinable()) {
    thread_.join();
  }
}

void VeiculoSimulado::conectar() {
  atualizar([](EstadoVeiculo& estado) { estado.modo_de_voo = "STABILIZE"; });
}

void VeiculoSimulado::iniciar_telemetria() {
  thread_ = std::thread(&VeiculoSimulado::laco_de_telemetria, this);
}

void VeiculoSimulado::laco_de_telemetria() {
  const Instante inicio = std::chrono::steady_clock::now();
  while (!parar_) {
    const double t = segundos_entre(inicio, std::chrono::steady_clock::now());
    atualizar([t](EstadoVeiculo& estado) {
      estado.atitude.rolagem = 25.0 * std::sin(t / 2.0);
      estado.atitude.arfagem = 10.0 * std::sin(t / 3.0);
      estado.atitude.guinada = std::fmod(t * 10.0, 360.0);
      estado.posicao.altitude_relativa_m = 10.0 + 5.0 * std::sin(t / 5.0);
      estado.bateria.tensao_v = 12.4;
      estado.bateria.percentual = std::max(0.0, 100.0 - t);
      estado.velocidade = 3.0 + 2.0 * std::sin(t / 4.0);
      estado.taxa_subida_ms = std::cos(t / 5.0); // derivada da altitude
      estado.rumo_graus = estado.atitude.guinada;
      estado.satelites = 12;
      estado.fix_gps = "Fix3D";
      estado.pronto_para_armar = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

void VeiculoSimulado::alterar_modo_de_voo(const std::string& modo) {
  std::string maiusculo;
  for (char caractere : modo) {
    maiusculo +=
        static_cast<char>(std::toupper(static_cast<unsigned char>(caractere)));
  }
  atualizar(
      [&maiusculo](EstadoVeiculo& estado) { estado.modo_de_voo = maiusculo; });
}

void VeiculoSimulado::armar() {
  atualizar([](EstadoVeiculo& e) { e.armado = true; });
}

void VeiculoSimulado::desarmar() {
  atualizar([](EstadoVeiculo& e) { e.armado = false; });
}

void VeiculoSimulado::decolar(double altura_m) {
  if (!estado().armado) {
    throw ErroDeVeiculo("não dá para decolar desarmado");
  }
  atualizar([altura_m](EstadoVeiculo& e) {
    e.em_voo = true;
    e.modo_de_voo = "GUIDED";
    e.posicao.altitude_relativa_m = altura_m;
  });
}

void VeiculoSimulado::pousar() {
  atualizar([](EstadoVeiculo& e) {
    e.em_voo = false;
    e.modo_de_voo = "LAND";
  });
}

void VeiculoSimulado::retornar_para_casa() {
  atualizar([](EstadoVeiculo& e) {
    e.em_voo = false;
    e.modo_de_voo = "RTL";
    e.posicao.latitude = 0.0;
    e.posicao.longitude = 0.0;
  });
}

void VeiculoSimulado::ir_para(Posicao pos) {
  if (!estado().em_voo) {
    throw ErroDeVeiculo("decole antes de ir para um ponto");
  }
  atualizar([pos](EstadoVeiculo& e) {
    e.posicao.latitude = pos.latitude;
    e.posicao.longitude = pos.longitude;
  });
}

} // namespace gcs
