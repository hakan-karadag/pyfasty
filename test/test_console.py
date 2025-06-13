#SURTOUT NE PAS MODIFIER CE FICHIER
import os
import sys


import pyfasty
from datetime import datetime
import time

class class_test_console:
    def __init__():
        pass

    def console_test_pyfasty():
        #default config
        pyfasty.console.config = {
                "console_view": True,    # Show/hide print messages
                "debug_view": True,    # Show/hide debug messages
                "format": "<!gray><%Y>-<%m>-<%d> <%H>:<%M>:<%S>.<%F:4> | <!reset><!type><%TYPE> <%FILE&%FUNC><!reset><!gray> | <!reset><%MESSAGE>",
                "colors": {
                  "type": {
                    "info": "\033[38;5;75m",
                    "success": "\033[38;5;82m",
                    "warning": "\033[38;5;220m",
                    "error": "\033[38;5;196m",
                    "debug": "\033[38;5;198m",
                    "critical": "\033[38;5;57m",
                    "fatal": "\033[48;5;196m\033[38;5;255m"
                  },
                  "gray": "\033[38;5;245m",
                  "reset": "\033[0m"
                },
                "save_log" : {
                    "status": True, #par defaut False
                    "filename": "log.txt",
                    "filemode": "w" #'a' : append (ajoute à la fin du fichier existant) et 'w' : write (écrase le fichier à chaque exécution)
                }
        }

        print(f"\n\033[96mTesting print functionality: (format: line 1 pyfasty, line 2 comparator)\033[0m")
        pyfasty.console.info(f"🔰 test console 1")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [INFO] [test_console.py:console_test_pyfasty] | 🔰 test console 1\n")
        
        pyfasty.console.success("🔰 test console 2")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [SUCCESS] [test_console.py:console_test_pyfasty] | 🔰 test console 2\n")
        
        pyfasty.console.warning("🔰 test console 3")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [WARNING] [test_console.py:console_test_pyfasty] | 🔰 test console 3\n")
        
        pyfasty.console.error("🔰 test console 4")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [ERROR] [test_console.py:console_test_pyfasty] | 🔰 test console 4\n")
        
        pyfasty.console.debug("🔰 test console 5")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [DEBUG] [test_console.py:console_test_pyfasty] | 🔰 test console 5\n")
        
        pyfasty.console.critical("🔰 test console 6")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [CRITICAL] [test_console.py:console_test_pyfasty] | 🔰 test console 6\n")
        
        pyfasty.console.fatal("🔰 test console 7")
        print(f"{datetime.now().strftime('%Y-%m-%d')} {datetime.now().strftime('%H:%M:%S.%f')[:-2]} | [FATAL] [test_console.py:console_test_pyfasty] | 🔰 test console 7\n")
        
        pyfasty.console("🔰 test console 8")
        print(f"🔰 test console 8\n")

        print("\n\033[96mTesting with debug disabled:\033[0m")
        pyfasty.console.config = {
                "debug_view": False,
                "format": "<%FUNC> <%FILE> <%TYPE> <%FUNC&%FILE> <%H>:<%M>:<%S> <%FILE&%FUNC> <%MESSAGE> <%MESSAGE>"
        }
        
        # Test 9: Les messages debug ne doivent pas apparaître
        pyfasty.console.debug("🔰 test console 9 - Should NOT appear")
        print(f"🔰 test console 9: Aucun message ne devrait être affiché au-dessus de cette ligne\n")
        
        # Test 10: Console désactivée
        pyfasty.console.config = {"console_view": False}
        pyfasty.console.info("🔰 test console 10 - Should NOT appear")
        print(f"🔰 test console 10: Aucun message ne devrait être affiché au-dessus de cette ligne\n")

        print("\n\033[96mTesting custom format:\033[0m")
        # Test 11: Console réactivée
        pyfasty.console.config = {"console_view": True}
        pyfasty.console.info(f"\n🔰 test console 11")
        print(f"[console_test_pyfasty] [test_console.py] [INFO] [test_console.py:console_test_pyfasty] {datetime.now().strftime('%H:%M:%S')} [test_console.py:console_test_pyfasty] \n🔰 test console 11 \n🔰 test console 11\n")

        class_test_console.console_benchmark_pyfasty()

    def console_benchmark_pyfasty():
        print("\n\033[96mBenchmark PyFasty Console vs Python Print\033[0m")

        # Sauvegarde de la configuration originale
        original_config = pyfasty.console.config.copy() if hasattr(pyfasty.console, "config") else {}

        # Configuration pour les benchmarks (pas d'affichage ni de logs)
        benchmark_config = {
            "console_view": False,
            "debug_view": False,
            "save_log": {"status": False}
        }

        # Test 1: Message simple
        iterations = 100000

        # Python natif avec redirection
        devnull = open(os.devnull, 'w')
        start_time = time.time()
        for i in range(iterations):
            print("Simple message test", file=devnull)
        native_time = time.time() - start_time
        devnull.close()

        # PyFasty
        pyfasty.console.config = benchmark_config
        start_time = time.time()
        for i in range(iterations):
            pyfasty.console.info("Simple message test")
        pyfasty_time = time.time() - start_time

        # Calcul du facteur d'accélération réel (combien de fois plus rapide)
        ratio = native_time / pyfasty_time

        print(f"  ⏳ Message simple - Python natif: {int(native_time*1000)}ms | PyFasty: {int(pyfasty_time*1000)}ms ({ratio:.1f}x plus rapide)")

        # Test 2: Message avec formatage variable
        iterations = 50000

        # Python natif
        devnull = open(os.devnull, 'w')
        start_time = time.time()
        for i in range(iterations):
            print(f"Test message with variable {i}", file=devnull)
        native_time = time.time() - start_time
        devnull.close()

        # PyFasty
        start_time = time.time()
        for i in range(iterations):
            pyfasty.console.info(f"Test message with variable {i}")
        pyfasty_time = time.time() - start_time

        # Calcul du facteur d'accélération réel (combien de fois plus rapide)
        ratio = native_time / pyfasty_time

        print(f"  ⏳ Formatage avec variables - Python natif: {int(native_time*1000)}ms | PyFasty: {int(pyfasty_time*1000)}ms ({ratio:.1f}x plus rapide)")

        # Test 3: Message avec horodatage
        iterations = 20000

        # Python natif avec formatage de date similaire
        devnull = open(os.devnull, 'w')
        start_time = time.time()
        for i in range(iterations):
            timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
            print(f"{timestamp} | [INFO] [benchmark.py] | Test message {i}", file=devnull)
        native_time = time.time() - start_time
        devnull.close()

        # PyFasty avec format similaire
        pyfasty.console.config = {
            "console_view": False,
            "debug_view": False,
            "format": "<%Y>-<%m>-<%d> <%H>:<%M>:<%S>.<%F:3> | <%TYPE> <%FILE> | <%MESSAGE>",
            "save_log": {"status": False}
        }

        start_time = time.time()
        for i in range(iterations):
            pyfasty.console.info(f"Test message {i}")
        pyfasty_time = time.time() - start_time

        # Calcul du facteur d'accélération réel (combien de fois plus rapide)
        ratio = native_time / pyfasty_time

        print(f"  ⏳ Horodatage formaté - Python natif: {int(native_time*1000)}ms | PyFasty: {int(pyfasty_time*1000)}ms ({ratio:.1f}x plus rapide)")

        # Test 4: Différents niveaux de logs
        iterations = 10000

        # Python natif
        devnull = open(os.devnull, 'w')
        start_time = time.time()
        for i in range(iterations):
            level = i % 6
            if level == 0:
                print("[INFO] Test message", file=devnull)
            elif level == 1:
                print("[SUCCESS] Test message", file=devnull)
            elif level == 2:
                print("[WARNING] Test message", file=devnull)
            elif level == 3:
                print("[ERROR] Test message", file=devnull)
            elif level == 4:
                print("[DEBUG] Test message", file=devnull)
            else:
                print("[CRITICAL] Test message", file=devnull)
        native_time = time.time() - start_time
        devnull.close()

        # PyFasty
        pyfasty.console.config = benchmark_config
        start_time = time.time()
        for i in range(iterations):
            level = i % 6
            if level == 0:
                pyfasty.console.info("Test message")
            elif level == 1:
                pyfasty.console.success("Test message")
            elif level == 2:
                pyfasty.console.warning("Test message")
            elif level == 3:
                pyfasty.console.error("Test message")
            elif level == 4:
                pyfasty.console.debug("Test message")
            else:
                pyfasty.console.critical("Test message")
        pyfasty_time = time.time() - start_time

        # Calcul du facteur d'accélération réel (combien de fois plus rapide)
        ratio = native_time / pyfasty_time

        print(f"  ⏳ Différents niveaux de logs - Python natif: {int(native_time*1000)}ms | PyFasty: {int(pyfasty_time*1000)}ms ({ratio:.1f}x plus rapide)")

        # Restauration de la configuration originale
        pyfasty.console.config = original_config

if __name__ == "__main__":
    class_test_console.console_test_pyfasty()