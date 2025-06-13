import os
import sys

import pyfasty

class class_test_config:
    def __init__():
        pass

    def config_test_pyfasty():

        pyfasty.config.app.name = "PyFasty Example"
        pyfasty.config.database.host = "localhost"
        pyfasty.config.database.port = 5432

        print("\n\033[96mConfiguration test pyfasty: (format: lib_pyfasty : expected_real_value)\033[0m")
        print(f"  {'✅' if str(pyfasty.config.app.name) == 'PyFasty Example' else '❌ Échec'} test config 1: {pyfasty.config.app.name} : PyFasty Example")
        print(f"  {'✅' if str(pyfasty.config.database.host) == 'localhost' else '❌ Échec'} test config 2: {pyfasty.config.database.host} : localhost")
        print(f"  {'✅' if str(pyfasty.config.database.port) == '5432' else '❌ Échec'} test config 3: {pyfasty.config.database.port} : {5432}")

        expected_config = {
            "model" : {
                "learning_rate": 0.001,
                "batch_size": 64,
                "epochs": 100
            },
            "database" : {
                "host": "127.0.0.1",
            }
        }

        pyfasty.config.model.learning_rate = 0.001
        pyfasty.config.model.batch_size = 64
        pyfasty.config.model.epochs = 100
        pyfasty.config.database.host = "127.0.0.1"

        filtered_config = {
            "model": {
                "learning_rate": float(pyfasty.config.model.learning_rate),
                "batch_size": int(pyfasty.config.model.batch_size),
                "epochs": int(pyfasty.config.model.epochs)
            },
            "database": {
                "host": str(pyfasty.config.database.host)
            }
        }

        print(f"  {'✅' if str(pyfasty.config.model.learning_rate) == '0.001' else '❌ Échec'} test config 5: {pyfasty.config.model.learning_rate} : 0.001")
        print(f"  {'✅' if str(pyfasty.config.model.batch_size) == '64' else '❌ Échec'} test config 6: {pyfasty.config.model.batch_size} : 64")
        print(f"  {'✅' if str(pyfasty.config.model.epochs) == '100' else '❌ Échec'} test config 7: {pyfasty.config.model.epochs} : 100")
        print(f"  {'✅' if str(pyfasty.config.app.name) == 'PyFasty Example' else '❌ Échec'} test config 8: {pyfasty.config.app.name} : PyFasty Example")
        print(f"  {'✅' if str(pyfasty.config.database.host) == '127.0.0.1' else '❌ Échec'} test config 9: {pyfasty.config.database.host} : 127.0.0.1")
        print(f"  {'✅' if str(pyfasty.config.database.port) == '5432' else '❌ Échec'} test config 10: {pyfasty.config.database.port} : {5432}")

        pyfasty.config.database.port = 0

        print(f"  {'✅' if str(pyfasty.config.database.port) == '0' else '❌ Échec'} test config 11: {pyfasty.config.database.port} : 0")

        pyfasty.config.test_config.test.test = True
        print(f"  {'✅' if str(pyfasty.config.test_config.test.test) == 'True' else '❌ Échec'} test config 12: {pyfasty.config.test_config.test.test} : True")

if __name__ == "__main__":
    class_test_config.config_test_pyfasty()