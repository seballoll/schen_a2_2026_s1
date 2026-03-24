# SPH Laboratory Restart

Proyecto reiniciado para implementar por secciones, con foco en laboratorio de paralelizacion.

## Documentos Base

- Pipeline unificado: docs/pipeline_unificado.md
- Plantilla de reporte por etapa: docs/plantilla_reporte_por_etapa.md
- Guia de modularidad y legibilidad: docs/guia_modularidad_y_legibilidad.md

## Contratos Minimos

- StageRunner (pipeline + metricas por etapa): include/simulation/StageRunnerContract.h
- Estrategia de hilos (puente para Sequential/FGMT/CGMT/SMT/CMP): include/threads/ThreadStrategyContract.h

## Estado actual

- Seccion 0: scaffold limpio
- Binario inicial: lab_runner

## Build

- cmake -S . -B build_root_test -DCMAKE_BUILD_TYPE=Release
- cmake --build build_root_test -j --target lab_runner

## Run

- ./build_root_test/Proyecto\ Individual/lab_runner
