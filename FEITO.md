# Feito (em cima do esqueleto)

Base: commit `6eff26f` (SkyDrones initial File). Trabalho em `d47bd14`
(formatação clang-format), `0d87593` (funcionalidades) e `9e9b0e8` (remove os
testes). `demo.sh` ainda não foi commitado.

1. **Modelo** (`modelo/`)
   - `EstadoVeiculo` ampliado: `velocidade`, `taxa_subida_ms`, `rumo_graus`,
     `satelites`, `fix_gps`, `sinal_radio_pct`, `pronto_para_armar`, `home`.
     `resumo()` mostra os campos novos (antes parava na bateria);
   - contrato de `Veiculo` declarado como virtual puro: `armar`, `desarmar`,
     `decolar`, `pousar`, `retornar_para_casa` e mais `ir_para(Posicao)`, que
     não estava na lista do esqueleto;
   - `Drone` implementa tudo com o plugin `Action`. `acao_sucesso()` converte o
     `Action::Result` em `ErroDeVeiculo`;
   - `Drone` assina telemetria nova: `velocity_ned` (velocidade horizontal e
     taxa de subida, convertida do NED), `heading`, `gps_info`, `rc_status`
     (NaN vira 0), `health` e `home`;
   - modos de voo: o esqueleto tinha 3 (`RTL`, `HOLD`, `LAND`) mapeados para
     ações do MAVSDK, e os outros davam `NaoImplementado`. Agora são 12 modos do
     ArduCopter (`STABILIZE`, `ACRO`, `ALT_HOLD`, `AUTO`, `GUIDED`, `LOITER`,
     `RTL`, `CIRCLE`, `LAND`, `POSHOLD`, `BRAKE`, `SMART_RTL`). Cada um é
     enviado como `MAV_CMD_DO_SET_MODE` pelo plugin `MavlinkDirect`. Modo
     desconhecido lança `ErroDeVeiculo` com a lista dos disponíveis;
   - `VeiculoSimulado`:
     - gera telemetria para os campos novos;
     - implementa as operações de voo: decolar desarmado ou ir para um ponto
       sem estar em voo lança erro;
     - não força mais `armado`/`em_voo = true` a cada ciclo, então esses
       estados agora vêm dos comandos;
   - **novo:** `Alarmes.{hpp,cpp}`, com `verificar_alarmes()` e
     `LimitesDeAlarme`: bateria < 20 %, rolagem ou arfagem > 30°,
     satélites < 6.

2. **Comandos** (`cli/ComandosPadrao.cpp`)
   - `decolar` implementado (antes lançava `NaoImplementado`): valida a altura
     (número, de 0 a 100 m), arma e decola;
   - comandos novos: `armar`, `desarmar`, `pousar`, `rtl`, `gravar <arquivo.csv>`
     e `limpar`;
   - `ir <lat> <lon> <alt_rel_m>`: valida as faixas e converte a altura
     relativa em absoluta usando a altitude de casa;
   - `status` também lista os alarmes ativos.

3. **Visualização** (`visual/`)
   - catálogo do `plotar` ganhou `velocidade`, `subida`, `rumo`, `satelites`,
     `rolagem` e `tensao`;
   - `Renderizador` desenha uma faixa vermelha com os alarmes embaixo de
     qualquer painel.

4. **Dados**
   - `Historico::exportar_csv()` grava todas as amostras: tempo, modo, armado,
     em voo, posição, atitude, velocidade, subida, bateria, tensão, satélites;
   - `EstacaoDeControle` ganhou `Historico& historico()` (não-`const`), usado
     por `gravar` e `limpar`.

5. **Extras**
   - `demo.sh` faz uma demonstração automática:
     - `./demo.sh` sobe o SITL sem MAVProxy, liga os fluxos `SR0_*` e voa o
       roteiro (modo guided, armar, decolar, `ir`, `rtl`, gravar CSV);
     - `./demo.sh --simulado` roda o mesmo roteiro com o `VeiculoSimulado`;
   - `CMakeLists.txt` inclui `Alarmes.cpp`;
   - código todo formatado com clang-format.

## Ainda pendente

- horizonte: escada de arfagem, arco de rolagem, fita de rumo;
- gráfico: ligar os pontos;
- exportar em JSON;
- desafio da interface gráfica (ImGui/ImPlot);
- testes de unidade para `Historico` e para os painéis. Foram escritos em
  `tests/testes.cpp` (`0d87593`) e removidos em `9e9b0e8`.
