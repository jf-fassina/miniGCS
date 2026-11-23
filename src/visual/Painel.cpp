#include "visual/Painel.hpp"

#include <iostream>
#include <thread>

#include "modelo/Alarmes.hpp"
#include "visual/Terminal.hpp"

namespace gcs {

Renderizador::Renderizador(const Historico& historico,
                           double quadros_por_segundo)
    : historico_(historico),
      intervalo_(std::chrono::milliseconds(static_cast<int>(
          1000.0 / (quadros_por_segundo > 0.0 ? quadros_por_segundo : 1.0)))) {}

void Renderizador::exibir_enquanto(const Painel& painel,
                                   const std::atomic<bool>& parar) const {
  std::cout << terminal::LIMPAR_TELA << terminal::ESCONDER_CURSOR;
  while (!parar) {
    std::cout << terminal::INICIO << painel.quadro(historico_);

    bool ok = false;
    const EstadoVeiculo atual = historico_.ultimo(&ok);
    const auto alarmes =
        ok ? verificar_alarmes(atual) : std::vector<std::string>{};
    std::cout << '\n' << terminal::LIMPAR_LINHA;
    if (!alarmes.empty()) {
      std::cout << terminal::NEGRITO << terminal::fundo(180, 0, 0)
                << terminal::frente(255, 255, 255);
      for (const auto& alarme : alarmes) {
        std::cout << " ⚠ " << alarme << ' ';
      }
      std::cout << terminal::NORMAL;
    }
    std::cout << std::flush;
    std::this_thread::sleep_for(intervalo_);
  }
  std::cout << terminal::MOSTRAR_CURSOR << terminal::LIMPAR_TELA << std::flush;
}

} // namespace gcs
