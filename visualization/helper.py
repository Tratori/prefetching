import os
import json

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")


def import_benchmark(name):
    benchmark = {}
    with open(os.path.join(DATA_DIR, name), "r") as fp:
        benchmark = json.load(fp)
    return benchmark
