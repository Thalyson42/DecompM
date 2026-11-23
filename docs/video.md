# Vídeo: interface e backends

Objetivo: o motor (`src/engine`) nunca chama OpenGL, GLES ou Vulkan. Ele só conhece `src/video/video.h`. Assim o mesmo código roda no PC e no
Android, e os backends entram um de cada vez: **Software (referência, já pronto)**, depois **GLES**, depois **Vulkan**.

## Contrato (`video::Video`)
- Recursos: `create_texture_indexed` (8 bits de índice), `create_texture_rgba`, `create_palette` (até 256 cores RGBA), `update_palette`.
- Quadro: `begin_frame(cor)` → `draw_sprite` / `fill_rect` → `end_frame`. Desenhar fora do par é ignorado.
- Coordenadas LÓGICAS (320x240 por padrão); o backend cuida da janela real (`resize`) e do letterbox.
- `read_pixels` devolve o quadro lógico em RGBA8, para testes e capturas.
- `SpriteDraw`: textura, paleta, posição do eixo, eixo do sprite, escala, ângulo, flipH/flipV, máscara, blend (Alpha/Add/Sub), alfa src/dst (0..256), PalFX, recorte.
- `create(Backend, &erro)`: devolve `nullptr` com uma mensagem quando o backend não foi compilado.

## O modelo vem do original
Os shaders que acompanham o MUGEN (`data/indexed.frag`, `data/rgba.frag`) mostram como ele desenha:
1. textura de índices (8 bits) + paleta numa textura 1D (`sampler1D`); em GLES 2/3 vira uma textura 2D de 256x1;
2. máscara: alfa 0 (índice 0 da paleta) descarta o pixel; sem máscara o alfa vira 1;
3. PalFX nessa ordem: saturação (`colorbal`) → inverter (`invertall`) → `(cor + add) * mul`;
4. alfa do sprite multiplicado por `src_alphamul`.
O backend de software segue essa ordem (`apply_palfx`).

## O que ainda NÃO está conferido (regra do projeto: o assembly manda)
- As fórmulas de blend Add/Sub/Alpha vêm da documentação do MUGEN. Falta ler no exe os `glBlendFunc` (ou o blitter de software) e os valores que o motor passa.
- Escala e rotação: o original usa vértices com perspectiva (`PERSPECTIVE_EVEN`); aqui usamos mapeamento inverso com amostragem por vizinho mais próximo.
- Sombras, reflexos e `Trans` do HitDef/Explod ainda não existem.

## Para o GLES e o Vulkan
- Não usar modo imediato nem `sampler1D`; shaders em GLSL ES (versão 100 ou 300). Para o Vulkan, os mesmos shaders compilados para SPIR-V (glslang, no build).
- Uma única textura 256x1 por paleta; `update_palette` troca só os dados (PalFX por paleta, e não por sprite).
- Texturas de sprite: NPOT sem mipmap, `CLAMP_TO_EDGE`, filtro `NEAREST`.
- A superfície nativa entra por `VideoConfig::nativeWindow` (SDL no PC, `ANativeWindow*` no Android); o contexto EGL/Vulkan fica DENTRO do backend, e o ciclo de vida (perder e recuperar a superfície no Android) chega por `resize` e `init`/`shutdown`.
