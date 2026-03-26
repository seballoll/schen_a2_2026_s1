BUILD_DIR := build_root_test
PYTHON ?= python3

.PHONY: setup-python build benchmark benchmark-quick

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
		--thread-counts 2,4 \
		--fgmt-quantum 12 \
		--out-dir benchmark_artifacts

benchmark-quick: setup-python build
	$(PYTHON) scripts/run_benchmark.py \
		--runner "$(BUILD_DIR)/Proyecto Individual/lab_runner" \
		--runs 20 \
		--steps 20 \
		--particles 200 \
		--thread-counts 2,4 \
		--fgmt-quantum 12 \
		--out-dir benchmark_artifacts/quick
