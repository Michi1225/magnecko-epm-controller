# LaTeX user guide

[`user-guide.tex`](user-guide.tex) is a standalone English guide for building, commissioning, and operating the Magnecko EPM controller. It covers the implemented EtherCAT interface, enable sequence, pulse commands, manual demagnetization, telemetry, fault handling, and known limitations.

It documents baseline commit `105e29d` and follows the Magnecko Sense manual's visual style: DejaVu Sans typography, Magnecko red chapter headings and links, a branded cover, document metadata, running headers and footers, and compact reference tables. The document identifier is `EPM-UG-001`, revision A; the cover records a firmware baseline rather than an unverified hardware revision.

Compile with LuaLaTeX (the default) or XeLaTeX and install the **DejaVu Sans** and **DejaVu Sans Mono** fonts. Required LaTeX packages are `fontspec`, `geometry`, `xcolor`, `microtype`, `amsmath`, `amssymb`, `array`, `booktabs`, `longtable`, `calc`, `graphicx`, `fvextra`, `enumitem`, `titlesec`, `fancyhdr`, `lastpage`, `etoolbox`, `xurl`, `hyperref`, and `bookmark`. No external images, bibliography, network access, or shell escape are required. The source is self-contained and does not depend on files in the Magnecko Sense repository.

## Build a PDF

With `lualatex` and Make available, run from the repository root:

```sh
make -C user-guide
```

To select XeLaTeX, run `make -B -C user-guide LATEX=xelatex`.

The output is `user-guide/build/user-guide.pdf`. The Makefile runs LaTeX three times to settle the contents layout and cross-references, and keeps its font cache under `build/texmf-var` so it can run without writing to a home-directory cache. To remove generated files:

```sh
make -C user-guide clean
```

Alternatively, from this folder:

```sh
mkdir -p build
lualatex -interaction=nonstopmode -halt-on-error -output-directory=build user-guide.tex
lualatex -interaction=nonstopmode -halt-on-error -output-directory=build user-guide.tex
lualatex -interaction=nonstopmode -halt-on-error -output-directory=build user-guide.tex
```

You can also upload `user-guide.tex` alone to a LaTeX editor and select LuaLaTeX or XeLaTeX with the DejaVu fonts installed. pdfLaTeX cannot compile the `fontspec` typography. Generated outputs under `build/` are ignored by Git. The detailed developer documentation starts at [wiki/Home.md](../wiki/Home.md).
