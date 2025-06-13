#SURTOUT NE PAS MODIFIER CE FICHIER
"""
Test des événements PyFasty

Ce module teste spécifiquement le système d'événements, en simulant une
application réelle utilisant la gestion de configuration et d'options.
"""

import pyfasty
import time
import sys

class config_event:
    def __init__():
        pass

    @pyfasty.event(lambda: pyfasty.config)
    def test_1_config_sync():
        pyfasty.registry.event_config_sync.all += 1 #valeur attendue : 2
        pyfasty.registry.event_config_sync.test_1_config += 1

    @pyfasty.event(lambda: pyfasty.config.event_test_2_sync)
    def test_2_config_sync():
        pyfasty.registry.event_config_sync.all += 1 #valeur attendue : 3
        pyfasty.registry.event_config_sync.test_2_config += 1

    @pyfasty.event(lambda: pyfasty.config.event_test_3_sync == "test")
    def test_3_config_sync():
        pyfasty.registry.event_config_sync.all += 1 #valeur attendue : 4
        pyfasty.registry.event_config_sync.test_3_config += 1

    @pyfasty.event(lambda: str(pyfasty.registry.event_options) == "primary" and pyfasty.config.event_test_3_sync == "test")
    def test_4_config_sync():
        pyfasty.registry.event_config_sync.all += 1 #valeur attendue : 5
        pyfasty.registry.event_config_sync.test_4_config += 1

    @pyfasty.event(lambda: pyfasty.config.event_fake_test_18)
    def fake_test_1_config_sync():
        pyfasty.registry.event_config_sync.all += 1 # ne devrait pas être appelé
        pyfasty.registry.event_config_sync.fake_test_1_config += 1

    @pyfasty.event(lambda: pyfasty.config.event_fake_test_18)
    def fake_test_1_config_async():
        pyfasty.registry.event_config_async.all += 1 # ne devrait pas être appelé
        pyfasty.registry.event_config_async.fake_test_1_config += 1

class registry_event:
    def __init__():
        pass

    @pyfasty.event(lambda: pyfasty.registry)
    def test_1_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_1_registry += 1

    @pyfasty.event(lambda: str(pyfasty.registry.event_options_sync) == "secondary")
    def test_2_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_2_registry += 1

    @pyfasty.event(lambda: pyfasty.registry.event_test_3_sync <= 10 and pyfasty.registry.event_test_3_sync >= 0 or False)
    def test_3_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_3_registry += 1

    @pyfasty.event(lambda: pyfasty.registry.event_test_4_sync == "488ADJ-87APMX-MMD999")
    def test_4_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_4_registry = "488ADJ-87APMX-MMD999-11ADD2"

    @pyfasty.event(lambda: pyfasty.registry.event_test_5_sync == "8458DD-NCHDDD-11ADD2" and 8770 == 8770 and True)
    def test_5_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_5_registry += 1

    @pyfasty.event(lambda: pyfasty.registry.event_test_6_async == 595626528985)
    def test_6_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_6_registry = 878453254845452

    @pyfasty.event(lambda: pyfasty.registry.event_test_7_sync == 0X1A55D)
    def test_7_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.test_7_registry = 0X99FF99

    @pyfasty.event(lambda: pyfasty.registry.fake_event_sync == 10)
    def fake_test_1_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.fake_test_1_registry += 1

    @pyfasty.event(lambda: pyfasty.registry.fake_event_test_19_sync == True)
    def fake_test_2_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.fake_test_2_registry += 1

    @pyfasty.event(lambda: pyfasty.registry.fake_event_test_20_sync == 5)
    def fake_test_3_registry_sync():
        pyfasty.registry.event_registry_sync.all += 1
        pyfasty.registry.event_registry_sync.fake_test_3_registry += 1

class class_test_event: 
    def event_test_pyfasty():

        pyfasty.registry.event_options = "primary"

        #test event config
        pyfasty.registry.event_config_sync.all = None
        for i in range(5):
            pyfasty.registry.event_config_sync[f"test_{i+1}_config"] = None

        pyfasty.config.event_test_2_sync = True
        pyfasty.config.event_test_3_sync = "test"
        

        print("\n\033[96mTest event config:\033[0m")
        print(f"  {'✅' if pyfasty.registry.event_config_sync.test_1_config > 1 else '❌ Échec'} test event 1: {pyfasty.registry.event_config_sync.test_1_config} : >1")
        print(f"  {'✅' if pyfasty.registry.event_config_sync.test_2_config == 1 else '❌ Échec'} test event 2: {pyfasty.registry.event_config_sync.test_2_config} : 1")
        print(f"  {'✅' if pyfasty.registry.event_config_sync.test_3_config == 1 else '❌ Échec'} test event 3: {pyfasty.registry.event_config_sync.test_3_config} : 1")
        print(f"  {'✅' if pyfasty.registry.event_config_sync.test_4_config == 1 else '❌ Échec'} test event 4: {pyfasty.registry.event_config_sync.test_4_config} : 1")
        
        print(f"  {'✅' if str(pyfasty.registry.event_config_sync.fake_test_1_config) == "None" else '❌ Échec'} test event 5: {pyfasty.registry.event_config_sync.fake_test_1_config} : None")
        
        print(f"  {'✅' if pyfasty.registry.event_config_sync.all > 1 else '❌ Échec'} test event 6: {pyfasty.registry.event_config_sync.all} : >1")

        #test event registry
        pyfasty.registry.event_registry_async.all = None
        for i in range(7):
            pyfasty.registry.event_registry_async[f"test_{i+1}_registry"] = None

        pyfasty.registry.event_options_sync = "secondary"
        pyfasty.registry.event_test_3_sync = 5
        pyfasty.registry.event_test_5_sync = f"8458DD-NCHDDD-11ADD2"
        pyfasty.registry.event_test_5_sync = f"8458DD-NCHDDD-11ADD2"
        pyfasty.registry.event_test_4_sync = "488ADJ-87APMX-MMD999"
        pyfasty.registry.event_test_6_async = 595626528985 
        pyfasty.registry.event_test_7_sync = 0X1A55D
        pyfasty.registry.event_test_5_sync = f"8458DD-NCHDDD-11ADD2"
            
        print(f"\npyfasty.registry.event_test_7_sync: {pyfasty.registry.event_test_7_sync} : {pyfasty.registry.event_test_7_sync == 0X1A55D}")

        print("\n\033[96mTest event registry:\033[0m")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_1_registry > 1 else '❌ Échec'} test event 1: {pyfasty.registry.event_registry_sync.test_1_registry} : >1")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_2_registry == 1 else '❌ Échec'} test event 2: {pyfasty.registry.event_registry_sync.test_2_registry} : 1")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_3_registry == 1 else '❌ Échec'} test event 3: {pyfasty.registry.event_registry_sync.test_3_registry} : 1")
        print(f"  {'✅' if str(pyfasty.registry.event_registry_sync.test_4_registry) == "488ADJ-87APMX-MMD999-11ADD2" else '❌ Échec'} test event 4: {pyfasty.registry.event_registry_sync.test_4_registry} : 488ADJ-87APMX-MMD999-11ADD2")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_5_registry == 1 else '❌ Échec'} test event 5: {pyfasty.registry.event_registry_sync.test_5_registry} : 1")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_6_registry == 878453254845452 else '❌ Échec'} test event 6: {pyfasty.registry.event_registry_sync.test_6_registry} : 878453254845452")
        print(f"  {'✅' if pyfasty.registry.event_registry_sync.test_7_registry == 0X99FF99 else '❌ Échec'} test event 7: {pyfasty.registry.event_registry_sync.test_7_registry} : 0X99FF99")

        print(f"  {'✅' if str(pyfasty.registry.event_registry_sync.fake_test_1_registry) == "None" else '❌ Échec'} test event 8: {pyfasty.registry.event_registry_sync.fake_test_1_registry} : None")
        print(f"  {'✅' if str(pyfasty.registry.event_registry_sync.fake_test_2_registry) == "None" else '❌ Échec'} test event 9: {pyfasty.registry.event_registry_sync.fake_test_2_registry} : None")
        print(f"  {'✅' if str(pyfasty.registry.event_registry_sync.fake_test_3_registry) == "None" else '❌ Échec'} test event 10: {pyfasty.registry.event_registry_sync.fake_test_3_registry} : None")

        print(f"  {'✅' if pyfasty.registry.event_registry_sync.all > 1 else '❌ Échec'} test event 12: {pyfasty.registry.event_registry_sync.all} : >1")

if __name__ == "__main__":
    # Exécuter directement si lancé comme script
    class_test_event.event_test_pyfasty()