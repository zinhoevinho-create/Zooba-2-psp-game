name: Gerar EBOOT

on: [push]

jobs:
  build:
    runs-on: ubuntu-latest
    container: pspdev/pspdev:latest
    steps:
      - uses: actions/checkout@v4

      - name: Compilar com Makefile
        run: make

      - name: Guardar EBOOT
        uses: actions/upload-artifact@v4
        with:
          name: Zooba2_EBOOT
          path: EBOOT.PBP

