#include "cli/ComandosPadrao.hpp"

#include <exception>
#include <iostream>
#include <memory>

#include "EstacaoDeControle.hpp"
#include "cli/Comando.hpp"
#include "modelo/Alarmes.hpp"
#include "modelo/Veiculo.hpp"
#include "visual/GraficoTempo.hpp"
#include "visual/HorizonteArtificial.hpp"

namespace gcs {
namespace {

// ---------------------------------------------------------------------------
// Comandos de exemplo. Cada um é uma classe pequena; o console não sabe quais
// existem, só pede ao registro o comando com o nome digitado.
// ---------------------------------------------------------------------------

class Ajuda : public Comando {
public:
  std::string nome() const override { return "ajuda"; }
  std::string ajuda() const override { return "lista os comandos disponíveis"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>&) override {
    std::cout << "Comandos:\n";
    for (const Comando* comando : estacao.comandos().todos()) {
      std::cout << "  " << comando->linha_de_ajuda() << '\n';
    }
  }
};

class Status : public Comando {
public:
  std::string nome() const override { return "status"; }
  std::string ajuda() const override {
    return "mostra o modo de voo e a telemetria atual";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>&) override {
    std::cout << estacao.veiculo() << '\n';
    for (const auto& alarme : verificar_alarmes(estacao.veiculo().estado()))
      std::cout << "  ALARME: " << alarme << '\n';
  }
};

class Modo : public Comando {
public:
  std::string nome() const override { return "modo"; }
  std::string uso() const override { return "modo <NOME>"; }
  std::string ajuda() const override {
    return "troca o modo de voo (ex.: modo RTL)";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (argumentos.size() != 1) {
      throw ErroDeComando("uso: " + uso());
    }
    estacao.veiculo().alterar_modo_de_voo(argumentos.front());
    std::cout << "pedido de troca de modo enviado\n";
  }
};

class Plotar : public Comando {
public:
  std::string nome() const override { return "plotar"; }
  std::string uso() const override { return "plotar <campo>"; }
  std::string ajuda() const override {
    return "abre o gráfico de um campo: " + nomes_dos_campos();
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (argumentos.size() != 1) {
      throw ErroDeComando("uso: " + uso());
    }
    const auto& catalogo = campos_disponiveis();
    const auto encontrado = catalogo.find(argumentos.front());
    if (encontrado == catalogo.end()) {
      throw ErroDeComando("campo '" + argumentos.front() +
                          "' não existe. Disponíveis: " + nomes_dos_campos());
    }
    const GraficoTempo grafico{encontrado->second};
    estacao.exibir_painel(grafico);
  }
};

class Horizonte : public Comando {
public:
  std::string nome() const override { return "horizonte"; }
  std::string ajuda() const override { return "abre o horizonte artificial"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>&) override {
    const HorizonteArtificial horizonte;
    estacao.exibir_painel(horizonte);
  }
};

class Decolar : public Comando {
public:
  std::string nome() const override { return "decolar"; }
  std::string uso() const override { return "decolar <altura_m>"; }
  std::string ajuda() const override {
    return "arma o drone e decola até a altura pedida";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& args) override {
    // TODO (aluno): validar a altura recebida, chamar veiculo().armar() e
    // veiculo().decolar(altura) — métodos que você vai declarar em Veiculo e
    // implementar em Drone — e avisar o usuário do resultado.
    if (args.size() != 1)
      throw ErroDeComando("uso: " + uso());
    double alt = 0;
    try {
      alt = std::stod(args[0]);
    } catch (const std::exception&) {
      throw ErroDeComando("altura inválida: " + args[0]);
    }
    if (alt <= 0.0 || alt > 100)
      throw ErroDeComando("altura deve estar entre 0 e 100m");

    estacao.veiculo().armar();
    estacao.veiculo().decolar(alt);
    std::cout << "decolando para " << alt << std::endl;
  }
};

class Armar : public Comando {
public:
  std::string nome() const override { return "armar"; }
  std::string ajuda() const override { return "arma os motores"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (!argumentos.empty())
      throw ErroDeComando("uso: " + uso());
    estacao.veiculo().armar();
    std::cout << "motores armados\n";
  }
};

class Desarmar : public Comando {
public:
  std::string nome() const override { return "desarmar"; }
  std::string ajuda() const override { return "desarma os motores"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (!argumentos.empty())
      throw ErroDeComando("uso: " + uso());
    estacao.veiculo().desarmar();
    std::cout << "motores desarmados\n";
  }
};

class Pousar : public Comando {
public:
  std::string nome() const override { return "pousar"; }
  std::string ajuda() const override { return "pousa no ponto atual"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (!argumentos.empty())
      throw ErroDeComando("uso: " + uso());
    estacao.veiculo().pousar();
    std::cout << "pousando\n";
  }
};

class Rtl : public Comando {
public:
  std::string nome() const override { return "rtl"; }
  std::string ajuda() const override {
    return "volta ao ponto de decolagem e pousa";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (!argumentos.empty())
      throw ErroDeComando("uso: " + uso());

    estacao.veiculo().retornar_para_casa();
    std::cout << "retornando para casa\n";
  }
};

class Ir : public Comando {
public:
  std::string nome() const override { return "ir"; }
  std::string uso() const override { return "ir <lat> <lon> <alt_rel_m>"; }
  std::string ajuda() const override {
    return "voa até a coordenada (altura relativa à decolagem)";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (argumentos.size() != 3)
      throw ErroDeComando("uso: " + uso());

    double lat = 0.0, lon = 0.0, alt = 0.0;
    try {
      lat = std::stod(argumentos[0]);
      lon = std::stod(argumentos[1]);
      alt = std::stod(argumentos[2]);
    } catch (const std::exception&) {
      throw ErroDeComando("números inválidos. uso: " + uso());
    }
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180)
      throw ErroDeComando("coordenada fora do intervalo");
    if (alt <= 0.0)
      throw ErroDeComando("altura deve ser positiva");

    const EstadoVeiculo atual = estacao.veiculo().estado();
    const double altitude_casa =
        atual.posicao.altitude_absoluta_m - atual.posicao.altitude_relativa_m;

    Posicao alvo;
    alvo.latitude = lat;
    alvo.longitude = lon;
    alvo.altitude_relativa_m = alt;
    alvo.altitude_absoluta_m = altitude_casa + alt;

    estacao.veiculo().ir_para(alvo);
    std::cout << "indo para " << lat << ", " << lon << " a " << alt << " m\n";
  }
};

class Sair : public Comando {
public:
  std::string nome() const override { return "sair"; }
  std::string ajuda() const override { return "encerra a estação de controle"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>&) override {
    estacao.encerrar();
  }
};

class Gravar : public Comando {
public:
  std::string nome() const override { return "gravar"; }
  std::string uso() const override { return "gravar <arquivo.csv>"; }
  std::string ajuda() const override { return "exporta o histórico para CSV"; }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (argumentos.size() != 1) {
      throw ErroDeComando("uso: " + uso());
    }
    estacao.historico().exportar_csv(argumentos[0]);
    std::cout << estacao.historico().tamanho() << " amostras gravadas em "
              << argumentos[0] << '\n';
  }
};

class Limpar : public Comando {
public:
  std::string nome() const override { return "limpar"; }
  std::string ajuda() const override {
    return "apaga o histórico dos gráficos";
  }

  void executar(EstacaoDeControle& estacao,
                const std::vector<std::string>& argumentos) override {
    if (!argumentos.empty()) {
      throw ErroDeComando("uso: " + uso());
    }
    estacao.historico().limpar();
    std::cout << "histórico apagado\n";
  }
};

} // namespace

RegistroDeComandos comandos_padrao() {
  RegistroDeComandos registro;
  registro.registrar(std::make_unique<Ajuda>());
  registro.registrar(std::make_unique<Status>());
  registro.registrar(std::make_unique<Modo>());
  registro.registrar(std::make_unique<Plotar>());
  registro.registrar(std::make_unique<Horizonte>());
  registro.registrar(std::make_unique<Decolar>());
  registro.registrar(std::make_unique<Sair>());
  registro.registrar(std::make_unique<Armar>());
  registro.registrar(std::make_unique<Desarmar>());
  registro.registrar(std::make_unique<Pousar>());
  registro.registrar(std::make_unique<Rtl>());
  registro.registrar(std::make_unique<Ir>());
  registro.registrar(std::make_unique<Gravar>());
  registro.registrar(std::make_unique<Limpar>());
  return registro;
}

} // namespace gcs
