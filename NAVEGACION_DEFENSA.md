# Navegacion para Defensa (Codigo + Teoria)

Este archivo esta pensado para que puedas moverte rapido durante la presentacion.
Cada item tiene una descripcion breve en lenguaje simple y un link directo al punto importante.

## 1) Teoria base que conviene abrir primero

Este documento explica el marco comun de comparacion entre modelos y por que el pipeline debe mantenerse igual.
- [Pipeline unificado y reglas de comparabilidad](Proyecto%20Individual/docs/pipeline_unificado.md#L1)

Aqui tienes una estructura de reporte por etapa para argumentar tiempo, ciclos, stalls y overhead sin improvisar.
- [Plantilla de reporte por etapa](Proyecto%20Individual/docs/plantilla_reporte_por_etapa.md#L1)

Esta guia te ayuda a defender decisiones de arquitectura y modularidad de forma ordenada.
- [Guia de modularidad y legibilidad](Proyecto%20Individual/docs/guia_modularidad_y_legibilidad.md#L1)

## 2) Contratos y arquitectura central

Si te preguntan por el diseno general, este es el punto de verdad del contrato de etapas y metricas.
- [Contrato de pipeline (StageId, StageMetrics, RunMetrics)](Proyecto%20Individual/include/simulation/StageRunnerContract.h#L11)

Este contrato define el puente para cambiar estrategia de ejecucion sin tocar la fisica.
- [Contrato de estrategia de hilos](Proyecto%20Individual/include/threads/ThreadStrategyContract.h#L10)

## 3) Flujo principal de ejecucion

Aqui se ve como se elige el modelo en runtime (sequential, fgmt, chunked, smt, cmp).
- [Seleccion de estrategia en main](Proyecto%20Individual/src/main.cpp#L127)

Aqui se imprime la tabla por etapa con wall time y desglose de ciclos para defensa.
- [Resumen de corrida y columnas de metricas](Proyecto%20Individual/src/main.cpp#L30)

## 4) Simulacion SPH por etapas (runner)

Este bloque muestra el ciclo de vida completo: initialize, runStep, runAllSteps.
- [Ciclo principal del runner](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L56)

Este punto muestra Stage 1, donde se construye la estructura espacial (uniform grid).
- [Stage 1: build de grid / neighbour structure](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L218)

Este punto muestra Stage 2, donde se calcula densidad/presion con consulta local por celdas vecinas.
- [Stage 2: density and pressure](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L246)

Este punto muestra Stage 3, donde se calculan fuerzas con el mismo enfoque de vecindad local.
- [Stage 3: forces](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L325)

## 5) Modelo teorico de ciclos y stalls (Bloque 1)

Aqui comienza el uso explicito de context switch, hidden stall y exposed stall en Stage 2.
- [Desglose de ciclos en Stage 2](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L303)

Aqui se aplica el mismo desglose para Stage 3.
- [Desglose de ciclos en Stage 3](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L403)

Esta funcion concentra la logica del modelo teorico por estrategia (sequential, fgmt, cgmt).
- [Modelo de stalls y switching por estrategia](Proyecto%20Individual/src/simulation/SequentialStageRunner.cpp#L596)

## 6) Estrategias simuladas de scheduling (comparacion justa de ciclos)

FGMT esta modelado con round-robin y quantum por items.
- [FGMT simulado (RR + quantum)](Proyecto%20Individual/src/threads/FGMTRoundRobinStrategy.cpp#L18)

CGMT esta modelado con particion coarse y cesion cooperativa ante stall sintetico.
- [CGMT simulado (coarse + yield por stall)](Proyecto%20Individual/src/threads/ParallelChunkedThreadStrategy.cpp#L16)

## 7) Estrategias reales para rendimiento de maquina (Bloque 2)

SMT real usa threads del SO y reparto dinamico por bloques pequenos.
- [SMT real (threads + asignacion dinamica)](Proyecto%20Individual/src/threads/SMTThreadStrategy.cpp#L18)

CMP real usa threads del SO y reparto estatico por chunks.
- [CMP real (threads + reparto estatico)](Proyecto%20Individual/src/threads/CMPThreadStrategy.cpp#L17)

## 8) Automatizacion de benchmark y perfilado (Bloque 3)

Este script corre experimentos repetidos y produce CSVs, CI95 y graficos.
- [Benchmark estadistico automatizado](scripts/run_benchmark.py#L57)

Este script corre perf para mediciones directas de maquina y exporta raw/summary.
- [Perfilado con perf y export CSV](scripts/run_perf_profile.py#L36)

Aqui tienes los targets para ejecutar build, benchmark y perf rapido/completo.
- [Comandos en Makefile](Makefile#L9)

## 9) Resultados recientes (atajos directos)

Si quieres mostrar evidencia inmediata en la defensa, abre estos archivos primero.
- [Resumen perf quick](benchmark_artifacts/perf_quick/perf_summary.csv)
- [Datos crudos perf quick](benchmark_artifacts/perf_quick/perf_raw.csv)
- [Resumen benchmark quick](benchmark_artifacts/quick/summary_stats.csv)
- [Convergencia benchmark quick](benchmark_artifacts/quick/convergence_report.txt)
