#!/usr/bin/env bash
# Demonstração automática da mini GCS.
#
#   ./demo.sh             voo completo no SITL (sobe o ArduCopter sozinho)
#   ./demo.sh --simulado  mesma GCS com o VeiculoSimulado, sem SITL
#
# O script digita os comandos no console da GCS, com pausas, como se fosse uma
# pessoa. Cada comando aparece em amarelo antes da resposta.
set -euo pipefail

RAIZ="$(cd "$(dirname "$0")" && pwd)"
ARDUPILOT="${ARDUPILOT:-$HOME/ardupilot}"
GCS="$RAIZ/build/mini_gcs"
SIMULADO=0
[[ "${1:-}" == "--simulado" ]] && SIMULADO=1

titulo() { printf '\n\033[1;36m=== %s ===\033[0m\n' "$1" >&2; }

# Envia um comando ao console e espera alguns segundos.
passo() {
  printf '\n\033[1;33m>>> %s\033[0m\n' "$1" >&2
  echo "$1"
  sleep "${2:-2}"
}

# Abre um painel em tela cheia por alguns segundos e volta (Enter).
painel() {
  passo "$1" "$2"
  echo
  sleep 1
}

titulo "Compilando"
cmake --build "$RAIZ/build" >/dev/null

# --------------------------------------------------------------------------
# Modo simulado: sem drone nenhum.
# --------------------------------------------------------------------------
if [[ $SIMULADO -eq 1 ]]; then
  {
    sleep 1
    titulo "1. Comandos disponíveis"
    passo "ajuda"
    titulo "2. Telemetria do veículo simulado"
    passo "status"
    titulo "3. Erros viram mensagens, o console não cai"
    passo "decolar abc"
    passo "decolar -5"
    titulo "4. Voo"
    passo "armar"
    passo "decolar 10"
    passo "status"
    painel "horizonte" 6
    painel "plotar rolagem" 6
    passo "pousar"
    passo "desarmar"
    titulo "5. Dados"
    passo "gravar voo_simulado.csv"
    passo "sair" 1
  } | "$GCS" --simulado
  titulo "Fim. Histórico gravado em voo_simulado.csv"
  exit 0
fi

# --------------------------------------------------------------------------
# Modo SITL: sobe o ArduCopter sem MAVProxy, então a GCS usa a porta 5760.
# --------------------------------------------------------------------------
BINARIO="$ARDUPILOT/build/sitl/bin/arducopter"
PARAMETROS="$ARDUPILOT/Tools/autotest/default_params/copter.parm"
if [[ ! -x "$BINARIO" ]]; then
  echo "SITL não encontrado em $BINARIO (defina ARDUPILOT=<caminho>)" >&2
  exit 1
fi
if ss -tln | grep -q ':5760 '; then
  echo "A porta 5760 já está em uso: feche o SITL/MAVProxy que está aberto." >&2
  exit 1
fi

PASTA_SITL="$(mktemp -d)"
# Sem o MAVProxy, ninguém pede ao ArduPilot que envie a telemetria pela porta
# 5760 (SERIAL0). Estes parâmetros ligam os fluxos (em Hz) logo na partida.
cat >"$PASTA_SITL/fluxos.parm" <<'PARAM'
SR0_RAW_SENS 2
SR0_EXT_STAT 2
SR0_RC_CHAN 2
SR0_POSITION 5
SR0_EXTRA1 10
SR0_EXTRA2 5
SR0_EXTRA3 2
PARAM
titulo "Subindo o SITL (ArduCopter) em $PASTA_SITL"
(cd "$PASTA_SITL" && exec "$BINARIO" -S --model + --speedup 1 \
  --defaults "$PARAMETROS,$PASTA_SITL/fluxos.parm" -I0 >sitl.log 2>&1) &
PID_SITL=$!
trap 'kill $PID_SITL 2>/dev/null; rm -rf "$PASTA_SITL"' EXIT

until ss -tln | grep -q ':5760 '; do sleep 0.5; done

{
  sleep 2
  titulo "1. Conectado: telemetria ao vivo"
  passo "status"
  titulo "Esperando o GPS e o EKF ficarem prontos (~30 s)"
  sleep 30
  passo "status"
  titulo "2. Erros viram mensagens, o console não cai"
  passo "decolar abc"
  titulo "3. Troca de modo pela própria GCS"
  passo "modo guided" 3
  passo "status"
  titulo "4. Armar e decolar"
  passo "armar" 3
  passo "decolar 10" 1
  painel "plotar altitude" 12 # mostra a subida ao vivo
  passo "status"
  painel "horizonte" 5
  titulo "5. Ir até um ponto ~80 m ao norte"
  passo "ir -35.36255 149.16523 15" 6
  passo "status" 12
  titulo "6. Voltar para casa"
  passo "rtl" 45
  passo "status"
  titulo "7. Dados"
  passo "gravar voo_sitl.csv"
  passo "sair" 1
} | "$GCS" --endereco tcp:127.0.0.1:5760

titulo "Fim. Histórico gravado em voo_sitl.csv"
