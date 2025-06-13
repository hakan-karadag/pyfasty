#!/usr/bin/env python3
import os
import sys
import shutil
import platform
import subprocess
from pathlib import Path
import glob

def run_command(cmd, cwd=None):
    """Exécute une commande shell et retourne son statut"""
    print(f"Exécution de: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    return result.returncode == 0

def uninstall():
    """Désinstalle le package, continue même en cas d'erreur"""
    print("=== Tentative de désinstallation du package existant ===")
    try:
        result = run_command([sys.executable, "-m", "pip", "uninstall", "-y", "pyfasty"])
        return result
    except Exception as e:
        print(f"Avertissement: Erreur lors de la désinstallation - {str(e)}")
        print("L'opération continue quand même...")
        return True  # Continue quand même

def build():
    """Compile le module d'extension"""
    print("=== Compilation du module d'extension ===")
    return run_command([sys.executable, "setup.py", "build_ext", "--inplace"])

def build_sdist():
    """Crée une distribution source qui peut être installée sur toutes les plateformes"""
    print("=== Création de la distribution source ===")
    return run_command([sys.executable, "setup.py", "sdist"])

def build_wheel():
    """Crée une wheel pour la plateforme actuelle"""
    print("=== Création de la wheel ===")
    return run_command([sys.executable, "setup.py", "bdist_wheel"])

def copy_module():
    """Copie le module compilé dans le répertoire pyfasty/"""
    print("=== Copie du module compilé ===")

    os.makedirs("pyfasty", exist_ok=True)

    if platform.system() == "Windows":
        pattern = "build/lib*/pyfasty/_pyfasty*.pyd"
    else:
        pattern = "build/lib*/pyfasty/_pyfasty*.so"

    found = False
    for path in Path(".").glob(pattern):
        shutil.copy2(path, "pyfasty/")
        print(f"Copié: {path} -> pyfasty/")
        found = True
    
    return found

def install():
    """Installe le package en mode développement"""
    print("=== Installation du package en mode développement ===")
    try:
        return run_command([sys.executable, "-m", "pip", "install", "-e", "."])
    except Exception as e:
        print(f"Avertissement: Erreur lors de l'installation - {str(e)}")
        print("Tentative d'installation avec --user...")
        return run_command([sys.executable, "-m", "pip", "install", "--user", "-e", "."])

def run_tests():
    """Exécute les tests"""
    print("=== Exécution des tests ===")
    return run_command([sys.executable, "main.py"], cwd="test")

def main():
    """Fonction principale qui exécute toutes les étapes de build automatiquement"""
    os_name = platform.system()
    print(f"Plateforme détectée: {os_name}")
    
    if os_name == "Windows":
        print("\nNote: Sur Windows, vous devrez peut-être exécuter ce script")
        print("avec des droits d'administrateur si Python est installé")
        print("dans C:\\Program Files.\n")
    
    print("\n=== PARTIE 1: BUILD ET INSTALLATION ===")

    uninstall()
    
    if not build():
        print("\n❌ Erreur lors de la compilation")
        return 1
    
    if not copy_module():
        print("\n❌ Erreur lors de la copie du module")
        return 1
    
    if not install():
        print("\n❌ Erreur lors de l'installation")
        return 1
    
    print("\n✅ Build et installation réussis!")
    
    # Étape 2: Créer les distributions pour toutes les plateformes
    print("\n=== PARTIE 2: CRÉATION DES DISTRIBUTIONS ===")
    dist_success = build_sdist() and build_wheel()
    
    if dist_success:
        print("\n✅ Distributions créées avec succès!")
        print("  - Source distribution (.tar.gz) dans dist/")
        print("  - Wheel (.whl) dans dist/")
        print("\nRemarque: Pour distribuer le package, vous devrez peut-être ajouter")
        print("tous les fichiers d'en-tête (.h) au MANIFEST.in pour l'installation à partir des sources.")
    else:
        print("\n❌ Erreur lors de la création des distributions")
        return 1
    
    print("\n=== PARTIE 3: EXÉCUTION DES TESTS FINAUX ===")
    if not run_tests():
        print("\n❌ Erreur lors des tests")
        return 1
    
    print("\n✅ Tests réussis avec succès!")
    return 0

if __name__ == "__main__":
    sys.exit(main()) 