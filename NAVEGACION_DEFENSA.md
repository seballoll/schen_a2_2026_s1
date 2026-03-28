# Navegacion para Defensa (Codigo + Teoria)

Guia rapida para moverte en vivo durante la exposicion sin perder tiempo.

## 0) Ruta sugerida de 8 minutos

1. Pipeline y comparabilidad.
- [Pipeline unificado](Proyecto%20Individual/docs/pipeline_unificado.md#L1)
2. Contratos de arquitectura (que cambia y que no cambia).
- [Contrato de etapas y metricas](Proyecto%20Individual/include/simulation/StageRunnerContract.h#L11)
- [Contrato de estrategia de hilos](Proyecto%20Individual/include/threads/ThreadStrategyContract.h#L10)
3. Seleccion de modelo en runtime.
- [Entrada principal del programa](Proyecto%20Individual/src/main.cpp#L116)
4. Pipeline por etapas con separadores visibles `========`.
- [Runner lifecycle y loop principal](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L58)
- [Dispatch por StageId](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L165)
5. Stage 1-6 (fisica + metricas).
- [Stage 1 Neighbour Structure](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L222)
- [Stage 2 Density Pressure](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L251)
- [Stage 3 Forces](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L331)
- [Stage 4 Integrate](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L432)
- [Stage 5 Boundary](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L473)
- [Stage 6 Metrics Snapshot](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L519)
6. Modelo teorico de ciclos/stalls.
- [Base work + stalls](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L573)
- [Breakdown por modelo](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L607)
7. Estrategias de hilos (simuladas y reales).
- [FGMT execution policy](Proyecto%20Individual/src/threads/FGMTRoundRobinStrategy.cpp#L21)
- [CGMT execution policy](Proyecto%20Individual/src/threads/ParallelChunkedThreadStrategy.cpp#L18)
- [SMT execution policy](Proyecto%20Individual/src/threads/SMTThreadStrategy.cpp#L20)
- [CMP execution policy](Proyecto%20Individual/src/threads/CMPThreadStrategy.cpp#L19)
8. Evidencia final y comandos.
- [Comandos de defensa (copy/paste)](COMANDOS_DEFENSA.md#L1)

## 1) Teoria base (abrir primero)

- [Pipeline unificado y reglas de comparabilidad](Proyecto%20Individual/docs/pipeline_unificado.md#L1)
- [Plantilla de reporte por etapa](Proyecto%20Individual/docs/plantilla_reporte_por_etapa.md#L1)
- [Guia de modularidad y legibilidad](Proyecto%20Individual/docs/guia_modularidad_y_legibilidad.md#L1)

## 2) Runner por etapas (con separadores `========`)

- [RUNNER LIFECYCLE](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L58)
- [PIPELINE DISPATCH](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L165)
- [STAGE 1](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L222)
- [STAGE 2](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L251)
- [STAGE 3](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L331)
- [STAGE 4](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L432)
- [STAGE 5](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L473)
- [STAGE 6](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L519)
- [SYNTHETIC CYCLE MODEL](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L573)
- [SYNTHETIC STALL BREAKDOWN](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L607)
- [SPATIAL HELPERS](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L707)

## 3) Estrategias de ejecucion (atajos)

- [Sequential model identification](Proyecto%20Individual/src/threads/SequentialThreadStrategy.cpp#L7)
- [Sequential execution policy](Proyecto%20Individual/src/threads/SequentialThreadStrategy.cpp#L17)
- [FGMT model identification](Proyecto%20Individual/src/threads/FGMTRoundRobinStrategy.cpp#L11)
- [FGMT execution policy](Proyecto%20Individual/src/threads/FGMTRoundRobinStrategy.cpp#L21)
- [CGMT model identification](Proyecto%20Individual/src/threads/ParallelChunkedThreadStrategy.cpp#L8)
- [CGMT execution policy](Proyecto%20Individual/src/threads/ParallelChunkedThreadStrategy.cpp#L18)
- [CGMT synthetic stall signal](Proyecto%20Individual/src/threads/ParallelChunkedThreadStrategy.cpp#L89)
- [SMT model identification](Proyecto%20Individual/src/threads/SMTThreadStrategy.cpp#L10)
- [SMT execution policy](Proyecto%20Individual/src/threads/SMTThreadStrategy.cpp#L20)
- [CMP model identification](Proyecto%20Individual/src/threads/CMPThreadStrategy.cpp#L9)
- [CMP execution policy](Proyecto%20Individual/src/threads/CMPThreadStrategy.cpp#L19)

## 4) Automatizacion y reproduccion

- [Benchmark args y modos de seed](scripts/run_benchmark.py#L38)
- [Benchmark main loop](scripts/run_benchmark.py#L473)
- [Perf args](scripts/run_perf_profile.py#L36)
- [Perf main](scripts/run_perf_profile.py#L254)

## 5) Evidencia final recomendada para mostrar

- [Cierre r30 seed fija](benchmark_artifacts/closing_fixed_r30)
- [Cierre r30 seed por ronda](benchmark_artifacts/closing_per_round_r30)
- [Topologia 2 hilos SMT vs CMP](benchmark_artifacts/smt_cmp_2thread_topology)
- [Precheck de entrega](ENTREGA_PRECHECK.md#L1)
- [Comandos de defensa](COMANDOS_DEFENSA.md#L1)

## 6) Orden de contingencia (si te cortan tiempo)

1. Mostrar arquitectura: contratos + main.
2. Mostrar runner en STAGE 2 y STAGE 3.
3. Mostrar una estrategia simulada (FGMT o CGMT) y una real (SMT o CMP).
4. Cerrar con `summary_stats.csv` y `convergence_report.txt` de las carpetas r30.
