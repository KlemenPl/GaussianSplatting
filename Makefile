.PHONY: web desktop clean

web:
	mkdir -p build_web
	cd build_web && \
	emscripten emcmake cmake -DPLATFORM=Web -DCMAKE_BUILD_TYPE=Release .. && \
	emscripten emmake make && \
	mv PixelDefense.html index.html && \
	rm -f ../PixelDefense.zip && \
	zip ../PixelDefense.zip index.html PixelDefense.*

desktop:
	mkdir -p build_desktop
	cd build_desktop && cmake -DCMAKE_BUILD_TYPE=Release ..
	cd build_desktop && make -j $(shell nproc)
	mkdir -p out_desktop/
	cp build_desktop/GaussianSplatting out_desktop/
	cp *.splat out_desktop/
	cp *.wgsl out_desktop/
	cp shader.wgsl out_desktop/
	cp imgui.ini out_desktop/

clean:
	rm -rf build_web
	rm -rf out_web
	rm -rf build_desktop
	rm -rf out_desktop
