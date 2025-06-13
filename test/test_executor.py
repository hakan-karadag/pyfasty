import os
import sys

import pyfasty
import time

# Ajouter le répertoire courant au sys.path
sys.path.insert(0, os.path.dirname(__file__))
from test_executor_2 import class_test_executor_2
class_test_executor_2 = class_test_executor_2

global global_test_classic_executor
global_test_classic_executor = 0

global global_test_return_executor_async
global_test_return_executor_async = False

global global_async_control
global_async_control = False

global complexe_structure_async
complexe_structure_async = False

class class_test_executor:
    def __init__():
        pass

    def executor_test_pyfasty():
        global global_test_classic_executor
        global global_test_return_executor_async
        global global_async_control
        global complexe_structure_async

        print(f"\n\033[96mTesting executor functionality: (format: resultat : valeur_attendue)\033[0m")
        #vue que je lance d'abord la fonction asynchrone, elle sera executee en premier et les print seront imbriqués
        
        class_test_executor.test_classic_executor()
        pyfasty.executor.sync.class_test_executor.test_classic_executor()
        pyfasty.executor._async.class_test_executor.test_classic_executor()
        time.sleep(0.1) #obligatoire pour que la valeur soit mise à jour vue que la fonction est asynchrone
        print(f"  {'✅' if global_test_classic_executor == 3 else '❌ Échec'} test executor 1: {global_test_classic_executor} : 3")

        value_return_classic = class_test_executor.test_return_executor()
        value_return_sync = pyfasty.executor.sync.class_test_executor.test_return_executor()
        pyfasty.executor._async.class_test_executor.test_return_executor_async()
        
        print(f"  {'✅' if value_return_classic == True else '❌ Échec'} test executor 1: {value_return_classic} : True")
        print(f"  {'✅' if value_return_sync == True else '❌ Échec'} test executor 2: {value_return_sync} : True")
        time.sleep(0.1) #obligatoire pour que la valeur soit mise à jour vue que la fonction est asynchrone
        print(f"  {'✅' if global_test_return_executor_async == True else '❌ Échec'} test executor 3: {global_test_return_executor_async} : True")

        pyfasty.executor._async.class_test_executor.test_async_executor_pyfasty()
        pyfasty.executor.sync.class_test_executor.test_sync_executor_pyfasty()
        time.sleep(0.2) #obligatoire pour que la valeur soit mise à jour vue que la fonction est asynchrone	
        print(f"  {'✅' if global_async_control == True else '❌ Échec'} test executor 4: {global_async_control} : True")

        complexe_structure_sync = pyfasty.executor.sync.class_test_executor.sync_test.aadgdre.dsfd()
        pyfasty.executor._async.class_test_executor.async_test.aadgdre.aa()
        print(f"  {'✅' if complexe_structure_sync == True else '❌ Échec'} test executor 5: {complexe_structure_sync} : True")
        time.sleep(0.1) #obligatoire pour que la valeur soit mise à jour vue que la fonction est asynchrone
        print(f"  {'✅' if complexe_structure_async == True else '❌ Échec'} test executor 6: {complexe_structure_async} : True")

        sync_test_2 = True
        sync_test_2 = pyfasty.executor.sync.class_test_executor_2.test_executor_2()
        async_test_2 = True
        async_test_2 = pyfasty.executor._async.class_test_executor_2.test_executor_2_async()
        time.sleep(0.1) #obligatoire pour que la valeur soit mise à jour vue que la fonction est asynchrone
        print(f"  {'✅' if sync_test_2 == True else '❌ Échec'} test executor 7: {sync_test_2} : True")
        print(f"  {'✅' if async_test_2 == True else '❌ Échec'} test executor 8: {async_test_2} : True")

        print(f"  {'✅' if sync_test_2 == pyfasty.executor.sync.class_test_executor.class_dependance_test_4.class_dependance_test_5.test_dependance() else '❌ Échec'} test executor 9: {pyfasty.executor.sync.class_test_executor.class_dependance_test_4.class_dependance_test_5.test_dependance()} : True")

        try:
            pyfasty.executor.sync.test_sync_executor_inexistant()
            print(f"  ❌ Échec test executor 10: Exception attendue mais non levée")
        except Exception as e:
            print(f"  ✅ test executor 10: Exception correctement levée : {e} comme attendu")
        
        try:
            pyfasty.executor._async.test_async_executor_inexistant()
            print(f"  ❌ Échec test executor 11: Exception attendue mais non levée") 
        except Exception as e:
            print(f"  ✅ test executor 11: Exception correctement levée : {e} comme attendu")

        class_test_executor.executor_benchmark_pyfasty()
        
    def test_classic_executor():
        global global_test_classic_executor
        global_test_classic_executor += 1
        print(f"  👁️ classic executor {global_test_classic_executor}/3")

    def test_return_executor():
        return True
    
    def test_return_executor_async():
        global global_test_return_executor_async
        global_test_return_executor_async = True
    
    def test_sync_executor_pyfasty():
        global global_async_control
        global_async_control = True

    def test_async_executor_pyfasty():
        global global_async_control
        time.sleep(0.1)
        global_async_control = True
    
    class sync_test:
        class aadgdre:
            def dsfd():
                return True

    class async_test:
        class aadgdre:
            def aa():
                global complexe_structure_async
                complexe_structure_async = True

    class class_dependance_test_1:
        class class_dependance_test_2:
            class class_dependance_test_3:
                def test_dependance():
                    return pyfasty.executor.sync.class_test_executor.class_dependance_test_4.class_dependance_test_5.test_dependance()
    
    class class_dependance_test_4:
        class class_dependance_test_5:
            def test_dependance():
                return True

    def executor_benchmark_pyfasty():
        print("\n\033[96mBenchmark PyFasty Executors vs Direct Calls\033[0m")

        # Fonction d'affichage des résultats
        def display_result(test_name, direct_ms, sync_ms, async_ms):
            sync_ratio = direct_ms / sync_ms if sync_ms > 0 else 0
            sync_status = "plus rapide" if sync_ratio > 1 else "plus lent"

            async_ratio = direct_ms / async_ms if async_ms > 0 else 0
            async_status = "plus rapide" if async_ratio > 1 else "plus lent"

            print(f"  ⏳ {test_name}:")
            print(f"     • Direct: {direct_ms}ms")
            print(f"     • Sync Executor: {sync_ms}ms ({sync_ratio:.1f}x {sync_status})")
            print(f"     • Async Executor: {async_ms}ms ({async_ratio:.1f}x {async_status})")

        # Fonction utilitaire pour mesurer avec phase d'échauffement
        def benchmark_with_warmup(func, warmup_iterations=1000, measured_iterations=10000):
            # Phase d'échauffement
            for _ in range(warmup_iterations):
                func()

            # Phase de mesure
            start_time = time.time()
            for _ in range(measured_iterations):
                func()
            end_time = time.time()

            execution_time = end_time - start_time
            return int(execution_time * 1000)

        counter = 0

        # Définir les fonctions de test dans le scope global
        global BenchClass
        class BenchClass:
            @staticmethod
            def simple_method():
                nonlocal counter
                counter += 1
                return counter

        def direct_call():
            return BenchClass.simple_method()

        def sync_executor_call():
            return pyfasty.executor.sync.BenchClass.simple_method()

        def async_executor_call():
            # Mesure juste l'enregistrement de la tâche (sans attente)
            pyfasty.executor._async.BenchClass.simple_method()
            return None

        # Mesure avec échauffement
        direct_ms = benchmark_with_warmup(direct_call, warmup_iterations=5000, measured_iterations=50000)
        sync_ms = benchmark_with_warmup(sync_executor_call, warmup_iterations=5000, measured_iterations=50000)
        async_ms = benchmark_with_warmup(async_executor_call, warmup_iterations=5000, measured_iterations=50000)

        display_result("Fonction simple avec retour direct", direct_ms, sync_ms, async_ms)

        global NestedStructure
        class NestedStructure:
            class Level1:
                class Level2:
                    class Level3:
                        @staticmethod
                        def nested_method():
                            return 42

        def direct_nested():
            return NestedStructure.Level1.Level2.Level3.nested_method()

        def sync_nested():
            return pyfasty.executor.sync.NestedStructure.Level1.Level2.Level3.nested_method()

        def async_nested():
            pyfasty.executor._async.NestedStructure.Level1.Level2.Level3.nested_method()
            return None

        # Mesure avec échauffement
        direct_ms = benchmark_with_warmup(direct_nested, warmup_iterations=2000, measured_iterations=20000)
        sync_ms = benchmark_with_warmup(sync_nested, warmup_iterations=2000, measured_iterations=20000)
        async_ms = benchmark_with_warmup(async_nested, warmup_iterations=2000, measured_iterations=20000)

        display_result("Accès à méthode dans structure imbriquée", direct_ms, sync_ms, async_ms)

        global benchmark_module
        class benchmark_module:
            @staticmethod
            def process_data(data_list):
                result = 0
                for i, value in enumerate(data_list):
                    result += value * (i + 1)
                return result

        test_data = [i for i in range(10)]

        def direct_process():
            return benchmark_module.process_data(test_data)

        def sync_process():
            return pyfasty.executor.sync.benchmark_module.process_data(test_data)

        def async_process():
            pyfasty.executor._async.benchmark_module.process_data(test_data)
            return None

        # Mesure avec échauffement
        direct_ms = benchmark_with_warmup(direct_process, warmup_iterations=1000, measured_iterations=10000)
        sync_ms = benchmark_with_warmup(sync_process, warmup_iterations=1000, measured_iterations=10000)
        async_ms = benchmark_with_warmup(async_process, warmup_iterations=1000, measured_iterations=10000)

        display_result("Traitement de données avec paramètres", direct_ms, sync_ms, async_ms)

        print("\n  📊 Résumé des performances:")
        print("     • Les appels synchrones via sync_executor ajoutent un overhead de 2-5x")
        print("     • Les appels asynchrones via async_executor sont généralement plus lents pour des opérations simples")
        print("     • Pour les opérations fréquentes et légères, préférez les appels directs")
        print("     • Pour les opérations lourdes ou en arrière-plan, l'overhead de l'executor devient négligeable")
        print("     • L'overhead du mécanisme d'introspection est significatif mais reste acceptable pour des opérations non critiques")
        print("     • L'overhead du mécanisme d'introspection est significatif mais reste acceptable pour des opérations non critiques")

if __name__ == "__main__":
    class_test_executor.executor_test_pyfasty()