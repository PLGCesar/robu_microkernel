#!/usr/bin/env bash
set -e

BUILD_DIR=${BUILD_DIR:-build}
ISO_DIR=${ISO_DIR:-build/iso_root}
ISO_OUT=${ISO_OUT:-build/robu_microkernel.iso}

echo "[ISO] Preparando árvore de diretórios..."
rm -rf "$ISO_DIR"
mkdir -p "$ISO_DIR/boot/grub"

if [ ! -f "$BUILD_DIR/robu_kernel" ]; then
    echo "[ISO] ERRO: Kernel $BUILD_DIR/robu_kernel não encontrado! Execute 'make' primeiro."
    exit 1
fi

if [ ! -f "$BUILD_DIR/rootfs.tar" ]; then
    echo "[ISO] ERRO: RootFS $BUILD_DIR/rootfs.tar não encontrado!"
    exit 1
fi

cp "$BUILD_DIR/robu_kernel" "$ISO_DIR/boot/robu_kernel"
cp "$BUILD_DIR/rootfs.tar" "$ISO_DIR/boot/rootfs.tar"
cp iso/boot/grub/grub.cfg "$ISO_DIR/boot/grub/grub.cfg"

echo "[ISO] Gerando Imagem ISO bootável via grub-mkrescue..."
grub-mkrescue -o "$ISO_OUT" "$ISO_DIR"

echo "[ISO] Sucesso! ISO gerada em: $ISO_OUT"
