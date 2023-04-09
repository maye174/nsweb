set -e

# cd to nsweb
cd "$(dirname $0)/.."

BASE_URL="https://github.com/xfangfang/wiliwili/releases/download/v0.1.0"
NSPMINI="switch-nspmini-48d4fc2-1-any.pkg.tar.xz"

if [ ! -f "${NSPMINI}" ];then
    wget ${BASE_URL}/${NSPMINI}
fi

dkp-pacman -U --noconfirm ${NSPMINI}

cmake -B build-docker -DBUILTIN_NSP=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
make -C build-docker -j$(nproc)