default: gen build

clean:
	rm -rf build

gen:
	cmake -Bbuild -S .
.PHONY: gen

build:
	cmake --build build
.PHONY: build
