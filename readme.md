<div align="center">
<h6>

ｒｕｄｅ　（育科せ）

</h6>

**[ a x11 floating wm ]**

[![License: Unlicense](https://img.shields.io/badge/License-Unlicense-pink.svg)](http://unlicense.org/)
[![Made with C](https://img.shields.io/badge/Made%20with-C-purple.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

</div>

## ✧ philosophy

rude is intentionally minimal. no borders, no bars, no distractions.
just you and your windows floating in space.

## ✧ preview

<p align="center">
    <img width="500" src="gifw-desk.gif" alt="thing">
</p>

## ✧ features

- 🪶 floating only
- ⚡ lightweight and fast
- 🎯 minimal dependencies
- 🔧 no config file

## ✧ installation

```bash
git clone https://github.com/getjared/rude.git
cd rude
make
sudo make install
```

## ✧ dependencies

- 📝 c compiler (gcc or clang)
- 🔧 make
- 🖥️ x11 libraries and headers (xlib)

## ✧ usage

### key bindings

| key | action |
|-----|--------|
| `mod4 + q` | kill focused window |
| `mod4 + mouse` | move windows around |

### starting rude

add to your `~/.xinitrc`:
```bash
exec rude
```

then:
```bash
startx
```

## ✧ configuration

rude follows the suckless philosophy - configuration is done in code.

1. modify source code
2. recompile
3. reinstall

### recommended tools

- `sxhkd` for additional keybindings
- `dmenu` for launching programs
- `st` for terminal emulation

## ✧ inspiration

crafted from scratch with inspiration from:
- sowm
- dwm

<div align="center">

```ascii
╭─────────────────────────╮
│  made with ♥ by jared   │
╰─────────────────────────╯
```

</div>
