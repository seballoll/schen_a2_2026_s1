BUILD_DIR := build_root_test
PYTHON ?= python3

.PHONY: setup-python build benchmark benchmark-quick perf-profile perf-profile-quick

setup-python:
	$(PYTHON) -m pip install --user --break-system-packages matplotlib

build:
	cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Release
	cmake --build $(BUILD_DIR) -j --target lab_runner

benchmark: setup-python build
	$(PYTHON) scripts/run_benchmark.py \
		--runner "$(BUILD_DIR)/Proyecto Individual/lab_runner" \
		--runs 200 \
		--steps 20 \
		--particles 200 \
		--thread-counts 2,4,6,8,10,12,14,16,18 \
		--fgmt-quantum 12 \
		--cmp-disable-smt \
		--smt-use-sudo \
		--out-dir benchmark_artifacts

benchmark-quick: setup-python build
	$(PYTHON) scripts/run_benchmark.py \
		--runner "$(BUILD_DIR)/Proyecto Individual/lab_runner" \
		--runs 20 \
		--steps 20 \
		--particles 200 \
		--thread-counts 2,4,6,8,10,12,14,16,18 \
		--fgmt-quantum 12 \
		--cmp-disable-smt \
		--smt-use-sudo \
		--out-dir benchmark_artifacts/quick

perf-profile: build
	$(PYTHON) scripts/run_perf_profile.py \
		--runner "$(BUILD_DIR)/Proyecto Individual/lab_runner" \
		--runs 30 \
		--steps 50 \
		--particles 1200 \
		--thread-counts 2,4,6,8,10,12,14,16,18 \
		--out-dir benchmark_artifacts/perf

perf-profile-quick: build
	$(PYTHON) scripts/run_perf_profile.py \
		--runner "$(BUILD_DIR)/Proyecto Individual/lab_runner" \
		--runs 5 \
		--steps 30 \
		--particles 800 \
		--thread-counts 2,4,6,8,10,12,14,16,18 \
		--out-dir benchmark_artifacts/perf_quick
