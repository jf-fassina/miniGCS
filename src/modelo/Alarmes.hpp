#pragma once

#include <string>
#include <vector>

#include "modelo/Telemetria.hpp"

namespace gcs {

struct LimitesDeAlarme {
  double bateria_minima_pct = 20.0;
  double rolagem_maxima_graus = 30.0;
  double arfagem_maxima_graus = 30.0;
  int satelites_minimos = 6;
};

std::vector<std::string> verificar_alarmes(const EstadoVeiculo& est,
                                           const LimitesDeAlarme& lim = {});

} // namespace gcs
