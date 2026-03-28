# Plantilla de Reporte por Etapa (Tiempo y Ciclos)

Objetivo: usar una sola plantilla para comparar modelos de forma clara y defendible.

## Portada de Corrida

- Fecha:
- Commit o version:
- Modelo evaluado: (Sequential, FGMT, CGMT, SMT, CMP)
- Parametros:
  - steps:
  - particles:
  - dt:
  - threads:
  - quantum_cycles:
  - context_switch_cycles:
  - stall_probability:
  - stall_latency_cycles:

## Resumen Global

- Tiempo total (wall):
- Ciclos totales simulados:
- Tiempo simulado (ciclos/clock):
- Speedup por tiempo vs baseline secuencial:
- Speedup por ciclos vs baseline secuencial:
- Eficiencia por tiempo:
- Eficiencia por ciclos:

## Tabla Principal por Etapa

Usar una fila por etapa:

- Stage
- Tiempo wall stage (ms)
- Porcentaje del tiempo total
- Ciclos de trabajo
- Ciclos idle
- Ciclos de context switch
- Stall cycles hidden
- Stall cycles exposed
- Stall cycles total
- Eventos de stall

Etapas esperadas:

- Stage 1 Neighbour Structure
- Stage 2 Density and Pressure
- Stage 3 Forces
- Stage 4 Integrate
- Stage 5 Boundary
- Stage 6 Metrics

## Tabla de Overhead

- Overhead fijo (setup, io, metricas finales)
- Overhead de scheduler
- Overhead de sincronizacion (barriers)
- Overhead por stalls expuestos
- Overhead total

## Validaciones de Coherencia

Checklist obligatorio:

- stall_total = stall_hidden + stall_exposed
- total_cycles = work + idle + context_switch
- Mismo input entre modelos comparados
- Baseline secuencial actualizado si cambia la fisica

## Seccion de Analisis

1. Hallazgos por etapa
- Que etapa domina tiempo
- Que etapa domina ciclos
- Donde hay mas stalls

2. Efecto de hilos
- Etapas que escalan bien
- Etapas limitadas por dependencia o barrera

3. Efecto del scheduler
- Impacto de quantum
- Impacto de context switch
- Balance hidden vs exposed stalls

4. Conclusiones
- Mejor configuracion para tiempo
- Mejor configuracion para ciclos
- Trade-off principal observado

## Seccion de Graficos (para carpeta charts)

Graficos minimos recomendados:

- Barras: tiempo por etapa y modelo
- Barras: ciclos por etapa y modelo
- Linea: speedup vs numero de hilos
- Stacked bars: work/idle/context-switch por modelo
- Stacked bars: hidden/exposed stalls por modelo

## Apendice de Reproducibilidad

- Comando de build:
- Comando de ejecucion:
- Archivo CSV generado:
- Semilla usada:
- Hardware usado:
