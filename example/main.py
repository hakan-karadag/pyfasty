import pyfasty

class init:

    def __init__():
        pass

    # Fonction globale pour l'initialisation
    @pyfasty.event_sync(lambda: pyfasty.config)
    def app_init():
        pyfasty.console.info("Initialisation de l'application...")
        pyfasty.console.info("Configuration : " + pyfasty.config.config)

class options:
    # Fonctions d'événement globales pour primary
    @pyfasty.event_sync(lambda: pyfasty.config.config == "primary" and str(pyfasty.registry.options["current"]) == "1")
    def primary_option_1():
        pyfasty.console.success("primary : Option 1 sélectionnée")

    @pyfasty.event_sync(lambda: pyfasty.config.config == "primary" and str(pyfasty.registry.options["current"]) == "2")
    def primary_option_2():
        pyfasty.console.warning("primary : Option 2 sélectionnée")

    # Fonctions d'événement globales pour secondary
    @pyfasty.event_sync(lambda: pyfasty.config.config == "secondary" and str(pyfasty.registry.options["current"]) == "1")
    def secondary_option_1():
        pyfasty.console.success("secondary : Option 1 sélectionnée")

    @pyfasty.event_sync(lambda: pyfasty.config.config == "secondary" and str(pyfasty.registry.options["current"]) == "2")
    def secondary_option_2():
        pyfasty.console.warning("secondary : Option 2 sélectionnée")

if __name__ == "__main__":
    
    # Cette ligne modifie la configuration en définissant config à "secondary"
    # → Déclenche le mécanisme de vérification d'événements pour le module CONFIG
    # → Tous les événements qui dépendent du module CONFIG dans leur condition sont évalués
    # → app_init() s'exécute car sa condition (lambda: pyfasty.config) est vraie
    # → Les conditions des événements primary_option_X et secondary_option_X sont également
    #   évaluées (puisqu'elles dépendent de config.config), mais ces fonctions ne s'exécutent pas
    #   car la partie registry.options["current"] de leurs conditions n'est pas satisfaite
    pyfasty.config.config = "secondary"
    
    # Cette ligne modifie le registre selon l'entrée utilisateur
    # → Déclenche le mécanisme de vérification d'événements pour le module REGISTRY
    # → Tous les événements qui dépendent du module REGISTRY dans leur condition sont évalués
    # → secondary_option_1() s'exécute si l'utilisateur entre "1" car sa condition complète
    #   est maintenant satisfaite (config.config == "secondary" ET registry.options["current"] == "1")
    # → app_init() ne s'exécute PAS car sa condition ne dépend que du module CONFIG et pas du module REGISTRY
    pyfasty.registry.options["current"] = input("Entrez une option : ")
    
    # Cette ligne modifie une autre valeur du registre
    # → Déclenche à nouveau le mécanisme de vérification pour le module REGISTRY
    # → Aucun événement ne s'exécute car aucune condition ne dépend spécifiquement de registry.test
    # → app_init() ne s'exécute PAS car sa condition ne dépend que du module CONFIG
    pyfasty.registry.test = "1"
    
    # Cette ligne appelle simplement la fonction console
    # → Aucun événement n'est déclenché car ce n'est pas une modification d'état
    # → Affiche simplement "test" dans la console
    pyfasty.console("test")