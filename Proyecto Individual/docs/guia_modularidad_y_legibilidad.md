# Guia de Modularidad y Legibilidad

Objetivo: mantener el codigo simple, corto y facil de presentar.

## Reglas de Diseno

- Un archivo, una responsabilidad clara.
- Funciones cortas: idealmente 20-40 lineas.
- Nombres descriptivos y consistentes.
- Evitar abreviaturas ambiguas.
- Evitar mezclar fisica con scheduler en la misma funcion.

## Estructura Recomendada

- simulation/: fisica y datos de simulacion.
- threads/: estrategias de ejecucion (secuencial, fgmt, cgmt, etc).
- visualization/: render y vistas.
- charts/: salida y scripts de graficos.
- src/: entrypoints y orquestacion minima.

## Capas Minimas

1. Capa de dominio
- datos de particula
- configuracion
- formulas fisicas

2. Capa de pipeline
- orden de etapas
- barreras
- contratos de entrada/salida por etapa

3. Capa de ejecucion
- como se reparte el trabajo entre hilos
- scheduler y quantum

4. Capa de metricas
- tiempo, ciclos y desglose por etapa

## Reglas para Mantener Simplicidad

- Primero que compile y sea correcto en secuencial.
- Luego paralelizar una sola etapa por iteracion.
- No agregar dos conceptos nuevos en el mismo commit.
- Antes de optimizar, medir.
- Si una funcion cuesta explicarla, dividirla.

## Convenciones de Estilo

- Preferir claridad sobre micro-optimizaciones.
- Comentarios solo cuando agregan contexto real.
- Evitar profundidad de anidamiento alta.
- Separar parametros de fisica y de scheduler.

## Definition of Done por Seccion

Una seccion se considera completa solo si:

- Compila.
- Tiene salida medible.
- Tiene una mini tabla de resultados.
- Mantiene el pipeline comun.
- Se puede explicar en menos de 3 minutos.
