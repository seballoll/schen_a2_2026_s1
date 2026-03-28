# Pipeline Unificado SPH (Base para Todas las Implementaciones)

Objetivo: usar el mismo flujo en todas las variantes (secuencial, FGMT, CGMT, SMT, CMP) para comparar de forma justa.

## Principios

- Mismo orden de etapas para todos los modelos.
- Mismo dataset de entrada por corrida (particulas, pasos, dt, semilla).
- Misma logica fisica por etapa.
- Solo cambia la estrategia de ejecucion de hilos.
- Barrera explicita entre etapas dependientes.

## Etapas del Pipeline

1. Stage 0 - Setup fijo
- Carga de parametros globales.
- Reserva de memoria y estructuras de apoyo.
- Inicializacion de particulas.
- Esta etapa no se usa para comparar speedup de hilos.

2. Stage 1 - Neighbour Structure
- Construccion o actualizacion de la estructura espacial.
- Salida: estructura lista para consultas.
- Naturaleza: parcial o serial segun diseno.

3. Stage 2 - Density and Pressure
- Por particula: consulta de vecinos y calculo de densidad/presion.
- Naturaleza: altamente paralelizable por particula.
- Requiere que Stage 1 haya finalizado.

4. Stage 3 - Forces
- Por particula: calculo de fuerzas internas y externas.
- Naturaleza: altamente paralelizable por particula.
- Requiere Stage 2 completo.

5. Stage 4 - Integrate
- Por particula: actualizacion de velocidad y posicion.
- Naturaleza: paralelizable.
- Requiere Stage 3 completo.

6. Stage 5 - Boundary
- Por particula: aplicacion de limites de dominio.
- Naturaleza: paralelizable.
- Requiere Stage 4 completo.

7. Stage 6 - Metrics
- Consolidacion de metricas de la iteracion y corrida.
- Naturaleza: serial o reduccion controlada.
- Debe ser identica entre modelos.

## Barreras Minimas Obligatorias

- Barrera despues de Stage 1
- Barrera despues de Stage 2
- Barrera despues de Stage 3
- Barrera despues de Stage 4
- Barrera despues de Stage 5

Razon: preservar correccion fisica y comparabilidad entre modelos.

## Politica de Overhead Fijo

Estas partes deben quedarse estables para todas las implementaciones:

- Inicializacion de estructuras de datos.
- Parseo de parametros y configuracion.
- Escritura de logs y reportes finales.
- Export a CSV y post-procesado.

El analisis de paralelizacion debe enfocarse en Stages 2-5.

## Contrato de Ejecucion por Modelo

Cada implementacion debe ejecutar exactamente:

- setup_fijo()
- for step in 0..N-1:
  - stage_1_neighbours()
  - barrier()
  - stage_2_density_pressure()
  - barrier()
  - stage_3_forces()
  - barrier()
  - stage_4_integrate()
  - barrier()
  - stage_5_boundary()
  - barrier()
  - stage_6_metrics_step()
- metrics_finalize_run()

Nota: barrier() puede ser no-op en secuencial.

## Reglas de Comparabilidad

- Mismo numero de pasos, particulas y dt.
- Misma semilla para eventos sinteticos (stalls).
- No mezclar cambios de fisica con cambios de threading.
- Si se cambia una formula fisica, repetir baseline secuencial.

## Evolucion Recomendada

- Version 1: solo secuencial, sin hilos.
- Version 2: paralelizar Stage 2.
- Version 3: paralelizar Stage 3.
- Version 4: paralelizar Stages 4-5.
- Version 5: introducir scheduler y quantum.
- Version 6: introducir stalls simulados y desglose por ciclos.
