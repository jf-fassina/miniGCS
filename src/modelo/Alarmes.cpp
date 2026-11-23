#include "modelo/Alarmes.hpp"

#include <cmath>

namespace gcs {

std::vector<std::string> verificar_alarmes(const EstadoVeiculo& estado,
                                           const LimitesDeAlarme& limites) {
  std::vector<std::string> alarmes;
  if (estado.bateria.percentual < limites.bateria_minima_pct) {
    alarmes.push_back("BATERIA BAIXA");
  }
  if (std::abs(estado.atitude.rolagem) > limites.rolagem_maxima_graus) {
    alarmes.push_back("ROLAGEM EXCESSIVA");
  }
  if (std::abs(estado.atitude.arfagem) > limites.arfagem_maxima_graus) {
    alarmes.push_back("ARFAGEM EXCESSIVA");
  }
  if (estado.satelites < limites.satelites_minimos) {
    alarmes.push_back("GPS FRACO");
  }
  return alarmes;
}

} // namespace gcs
