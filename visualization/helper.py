import os
import json
import pandas as pd
import copy

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data")


def import_benchmark(name):
    benchmark = {}
    with open(os.path.join(DATA_DIR, name), "r") as fp:
        benchmark = json.load(fp)
    return benchmark


def load_flat_jsons(directory):
    all_results = []
    for filename in os.listdir(directory):
        if filename.endswith(".json"):
            filepath = os.path.join(directory, filename)
            data = import_benchmark(filepath)
            for result in data["results"]:
                result["id"] = filename[: -len(".json")]
            all_results.extend(data["results"])
    return all_results


def load_results_jsons(directory):
    all_results = []
    for node_folder in os.listdir(directory):
        node_path = os.path.join(directory, node_folder)
        if not os.path.isdir(node_path):
            continue

        for filename in os.listdir(node_path):
            if filename.endswith(".json"):
                filepath = os.path.join(node_path, filename)
                data = import_benchmark(filepath)
                for result in data["results"]:
                    result["id"] = node_folder
                all_results.extend(data["results"])
    return all_results


def build_1_d_json(data, d_json, base_key=""):
    if isinstance(data, dict):
        for key, value in data.items():
            if isinstance(value, dict):
                build_1_d_json(
                    value, d_json, key if base_key == "" else base_key + "." + key
                )
            else:
                d_json[key if (base_key == "") else base_key + "." + key] = []
    return d_json


def fill_schema(schema, result, base_key=""):
    for key, value in result.items():
        if isinstance(value, dict):
            fill_schema(schema, value, key if base_key == "" else base_key + "." + key)
        else:
            schema[key if base_key == "" else base_key + "." + key].append(value)


def load_flat_benchmark_directory_to_pandas(path):
    results = load_flat_jsons(path)
    schema = build_1_d_json(results[0], {})
    for result in results:
        fill_schema(schema, result)
    return pd.DataFrame(schema)


def load_results_benchmark_directory_to_pandas(path):
    results = load_results_jsons(path)
    schema = build_1_d_json(results[0], {})
    for result in results:
        fill_schema(schema, result)
    return pd.DataFrame(schema)


if __name__ == "__main__":
    df = load_flat_benchmark_directory_to_pandas(f"{DATA_DIR}/lfb_batched")
    print(df)
