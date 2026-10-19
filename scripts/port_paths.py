"""Portable paths shared by the installer and optional GPU probes (stdlib only)."""
from pathlib import Path
import os
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def tool(name):
    found = shutil.which(name)
    if found:
        return found
    fallback = Path('/opt/homebrew/bin') / name
    if fallback.is_file():
        return str(fallback)
    raise SystemExit(f'Falta {name}; ejecuta ./setup.sh')


def sdk():
    choices = [Path(os.environ['VULKAN_SDK'])] if os.environ.get('VULKAN_SDK') else []
    choices += [ROOT / '.tools/vulkan-sdk/macOS', ROOT / 'vulkan-sdk/macOS']
    for path in choices:
        if (path / 'bin/dxc').is_file():
            return path.resolve()
    raise SystemExit('Falta Vulkan SDK; ejecuta ./setup.sh o define VULKAN_SDK')


def java_tool(name):
    choices = []
    if os.environ.get('JAVA_HOME'):
        choices.append(Path(os.environ['JAVA_HOME']) / 'bin' / name)
    if shutil.which('brew'):
        result = subprocess.run(['brew', '--prefix', 'openjdk@21'], capture_output=True, text=True)
        if result.returncode == 0:
            choices.append(Path(result.stdout.strip()) / 'bin' / name)
    choices.append(Path('/opt/homebrew/opt/openjdk@21/bin') / name)
    choices += [Path(p) / name for p in os.environ.get('PATH', '').split(os.pathsep)]
    for path in choices:
        if path.is_file():
            return str(path)
    raise SystemExit(f'Falta Java 21 ({name}); ejecuta ./setup.sh')


def require_game_closed():
    processes = subprocess.run([java_tool('jps'), '-l'], check=True, capture_output=True, text=True).stdout.lower()
    if any(name in processes for name in ('net.minecraft', 'net.fabricmc', 'org.prismlauncher', 'org.multimc')):
        raise SystemExit('Cierra Minecraft antes de instalar o cambiar sus ajustes')


def minecraft_base():
    return Path(os.environ.get('MINECRAFT_HOME', str(Path.home() / 'Library/Application Support/minecraft'))).expanduser()
