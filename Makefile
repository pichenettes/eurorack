BUILD_DIR=build

default: gen build

clean:
	rm -rf ${BUILD_DIR}

gen:
	cmake -B ${BUILD_DIR} -S .

build:
	cmake --build ${BUILD_DIR}

.PHONY: clean gen build
