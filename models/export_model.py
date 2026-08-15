"""One-time offline export of all-MiniLM-L6-v2 to ONNX.

Exports only the raw transformer body (input_ids, attention_mask ->
last_hidden_state). Mean-pooling and normalization are done ourselves
in C++, not baked into the graph, matching the project's design.
"""

import torch
from transformers import AutoModel, AutoTokenizer

MODEL_NAME = "sentence-transformers/all-MiniLM-L6-v2"
OUTPUT_DIR = "onnx_export"

tokenizer = AutoTokenizer.from_pretrained(MODEL_NAME)
model = AutoModel.from_pretrained(MODEL_NAME)
model.eval()

sample = tokenizer(["hello world"], padding=True, truncation=True, return_tensors="pt")

torch.onnx.export(
    model,
    (sample["input_ids"], sample["attention_mask"]),
    f"{OUTPUT_DIR}/all-MiniLM-L6-v2.onnx",
    input_names=["input_ids", "attention_mask"],
    output_names=["last_hidden_state"],
    dynamic_axes={
        "input_ids": {0: "batch", 1: "sequence"},
        "attention_mask": {0: "batch", 1: "sequence"},
        "last_hidden_state": {0: "batch", 1: "sequence"},
    },
    opset_version=14,
)

tokenizer.save_vocabulary(OUTPUT_DIR)

print("exported model + vocab to", OUTPUT_DIR)
