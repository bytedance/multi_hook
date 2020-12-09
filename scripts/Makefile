

.PHONY: build install uninstall

build: 
	./make_multi_hook_dkms_deb.sh

install: build
	dpkg -i ./multi-hook-dkms_1.0.0_amd64.deb

uninstall:
	-dpkg -r multi-hook-dkms
	-rm multi-hook-dkms_1.0.0_amd64.deb
# 	-rm -r ../deb