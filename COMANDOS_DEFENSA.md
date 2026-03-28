# Comandos de Defensa (Copy/Paste)

Guia practica para ejecutar benchmarks y generar artefactos (CSV + graficos) el dia de la defensa.

## 1) Pre-chequeo rapido

Ejecutar desde la raiz del repo.

```bash
cd "/home/sebas/Documents/GitHub/schen_a2_2026_s1"
```

Verificar binario:

```bash
ls -lh "./Proyecto Individual/build/lab_runner"
```

Si falta el binario, compilar rapido:

```bash
cmake -S . -B build_root_test -DCMAKE_BUILD_TYPE=Release
cmake --build build_root_test -j
```

## 2) Ejecuciones individuales por estrategia (heavy)

### 2.1 SMT heavy

```bash
./scripts/run_smt_heavy.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts/smt_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18"
```

Salida esperada:
- benchmark_artifacts/smt_heavy/raw_smt_runs.csv
- benchmark_artifacts/smt_heavy/summary_smt.csv
- benchmark_artifacts/smt_heavy/smt_heavy_trend.png

### 2.2 FGMT heavy

```bash
./scripts/run_fgmt_heavy.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts/fgmt_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18" 12
```

Salida esperada:
- benchmark_artifacts/fgmt_heavy/raw_fgmt_runs.csv
- benchmark_artifacts/fgmt_heavy/summary_fgmt.csv
- benchmark_artifacts/fgmt_heavy/fgmt_heavy_trend.png

### 2.3 CGMT (chunked) heavy

```bash
./scripts/run_cgmt_heavy.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts/cgmt_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18"
```

Salida esperada:
- benchmark_artifacts/cgmt_heavy/raw_cgmt_runs.csv
- benchmark_artifacts/cgmt_heavy/summary_cgmt.csv
- benchmark_artifacts/cgmt_heavy/cgmt_heavy_trend.png

### 2.4 CMP heavy (con SMT apagado durante CMP)

```bash
./scripts/run_cmp_heavy.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts/cmp_heavy 12 50 1200 "2,4,6,8,10,12,14,16,18" yes
```

Nota: este script usa sudo para tocar /sys/devices/system/cpu/smt/control si hace falta.

Salida esperada:
- benchmark_artifacts/cmp_heavy/raw_cmp_runs.csv
- benchmark_artifacts/cmp_heavy/summary_cmp.csv
- benchmark_artifacts/cmp_heavy/cmp_heavy_trend.png

## 3) Comparativa topologica 2 hilos (SMT mismo core vs CMP distinto core)

Auto-deteccion de CPUs:

```bash
sudo ./scripts/run_smt_cmp_2thread_topology.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts/smt_cmp_2thread_topology \
  20 50 1200 "" "" yes
```

Salida esperada:
- benchmark_artifacts/smt_cmp_2thread_topology/raw_smt_cmp_2thread.csv
- benchmark_artifacts/smt_cmp_2thread_topology/summary_smt_cmp_2thread.csv
- benchmark_artifacts/smt_cmp_2thread_topology/smt_cmp_2thread_wall.png
- benchmark_artifacts/smt_cmp_2thread_topology/smt_cmp_2thread_cycles.png

## 4) Campanas integradas (todas las estrategias, con graficos)

## 4.1 Campana integrada estandar (12 runs)

```bash
python3 scripts/run_benchmark.py \
  --runner "./Proyecto Individual/build/lab_runner" \
  --runs 12 \
  --steps 50 \
  --particles 1200 \
  --thread-counts 2,4,6,8,10,12,14,16,18 \
  --fgmt-quantum 12 \
  --seed-start 1 \
  --seed-mode per-round \
  --out-dir benchmark_artifacts/pre_delivery_suite_v3
```

## 4.2 Campana de cierre r30 (sin tocar SMT)

### A) Seed fija (control de ruido)

```bash
python3 scripts/run_benchmark.py \
  --runner "./Proyecto Individual/build/lab_runner" \
  --runs 30 \
  --steps 50 \
  --particles 1200 \
  --thread-counts 2,4,6,8,10,12,14,16,18 \
  --fgmt-quantum 12 \
  --seed-start 1 \
  --seed-mode fixed \
  --out-dir benchmark_artifacts/closing_fixed_r30
```

### B) Seed por ronda (robustez)

```bash
python3 scripts/run_benchmark.py \
  --runner "./Proyecto Individual/build/lab_runner" \
  --runs 30 \
  --steps 50 \
  --particles 1200 \
  --thread-counts 2,4,6,8,10,12,14,16,18 \
  --fgmt-quantum 12 \
  --seed-start 1 \
  --seed-mode per-round \
  --out-dir benchmark_artifacts/closing_per_round_r30
```

Ambas generan:
- raw_runs.csv
- summary_stats.csv
- convergence_report.txt
- cycles_trend_fgmt_cgmt_vs_sequential.png
- wall_time_trend_fgmt_cgmt_vs_sequential.png
- cycles_trend_smt_cmp_vs_sequential.png
- wall_time_trend_smt_cmp.png

## 5) Campanas r30 con SMT apagado durante CMP (recomendado)

Se agrego script dedicado para no olvidarse de flags.

```bash
sudo ./scripts/run_benchmark_r30_smt_off.sh \
  "./Proyecto Individual/build/lab_runner" \
  benchmark_artifacts \
  50 \
  1200 \
  "2,4,6,8,10,12,14,16,18" \
  12 \
  1
```

Salidas:
- benchmark_artifacts/closing_fixed_r30_smt_off
- benchmark_artifacts/closing_per_round_r30_smt_off

Cada carpeta con CSV + convergencia + 4 graficos.

## 6) Perfilado de hardware con perf (SMT/CMP)

```bash
python3 scripts/run_perf_profile.py \
  --runner "./Proyecto Individual/build/lab_runner" \
  --runs 10 \
  --steps 50 \
  --particles 1200 \
  --thread-counts 2,4,6,8 \
  --out-dir benchmark_artifacts/perf_defensa
```

Si perf no esta disponible, deja aviso en perf_warning.txt.

## 7) Comandos de verificacion rapida para mostrar en vivo

Ver carpetas clave:

```bash
ls -lah benchmark_artifacts/closing_fixed_r30
ls -lah benchmark_artifacts/closing_per_round_r30
ls -lah benchmark_artifacts/smt_cmp_2thread_topology
```

Contar configuraciones estables (CI95/mean <= 2%):

```bash
awk '/-> OK/{ok++} /MORE_RUNS_RECOMMENDED/{more++} END{print "OK="ok" MORE="more}' \
  benchmark_artifacts/closing_fixed_r30/convergence_report.txt

awk '/-> OK/{ok++} /MORE_RUNS_RECOMMENDED/{more++} END{print "OK="ok" MORE="more}' \
  benchmark_artifacts/closing_per_round_r30/convergence_report.txt
```

## 8) Orden sugerido para la defensa (minimo riesgo)

1. Ejecutar topologia 2 hilos (resultado rapido y claro).
2. Mostrar una corrida individual (por ejemplo SMT heavy o FGMT heavy).
3. Mostrar resultados ya generados de r30 (fixed y per-round).
4. Si hay tiempo, lanzar run_perf_profile.py para reforzar analisis de hardware.

## 9) Errores comunes y solucion rapida

- Error de sudo no interactivo (-n): ejecutar scripts de SMT toggle con sudo completo.
- Ruta con espacio en Proyecto Individual: siempre comillar runner.
- Falta de matplotlib: se generan CSV igual; el PNG puede omitirse con warning.
- SMT control no escribible: usar sudo para scripts que cambian /sys/devices/system/cpu/smt/control.
