

.PHONY: build install uninstall

build: 
#	./make_multi_hook_dkms_deb.sh
	./make_multi_hook_dkms_deb1.sh

install: build
	dpkg -i ./multi-hook-dkms_1.3.0_amd64.deb

uninstall:
	-dpkg -r multi-hook-dkms
	-rm multi-hook-dkms_1.3.0_amd64.deb
# 	-rm -r ../deb
