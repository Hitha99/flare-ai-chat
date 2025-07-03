# -*- coding: utf-8 -*-
import json
from datasets import Dataset

with open("Talk2Car/data/commands/train_commands.json") as f:
    raw_data = json.load(f)

prompts = [item["command"] for item in raw_data["commands"][:500]]  # limit to 500 for Colab memory
responses = [f"Executing: {cmd}" for cmd in prompts]

# Create HuggingFace dataset
dataset = Dataset.from_dict({"prompt": prompts, "response": responses})
dataset = dataset.train_test_split(test_size=0.2)

def format_prompt(example):
    return {
        "text": f"### Instruction:\n{example['prompt']}\n\n### Response:\n{example['response']}"
    }

dataset = dataset.map(format_prompt)

from transformers import AutoTokenizer, AutoModelForCausalLM

model_name = "distilgpt2"
tokenizer = AutoTokenizer.from_pretrained(model_name)
tokenizer.pad_token = tokenizer.eos_token
model = AutoModelForCausalLM.from_pretrained(model_name)

from peft import get_peft_model, LoraConfig, TaskType

peft_config = LoraConfig(
    task_type=TaskType.CAUSAL_LM,
    r=8,
    lora_alpha=16,
    lora_dropout=0.1,
    bias="none"
)

model = get_peft_model(model, peft_config)
model.print_trainable_parameters()

def tokenize(example):
    return tokenizer(example["text"], padding="max_length", truncation=True, max_length=128)

tokenized_dataset = dataset.map(tokenize)

# dataset["train"][0]

from transformers import TrainingArguments, Trainer, DataCollatorForLanguageModeling

training_args = TrainingArguments(
    output_dir="./lora_output",
    per_device_train_batch_size=4,
    num_train_epochs=3,
    learning_rate=2e-4,
    logging_steps=10,
    save_strategy="epoch",
    report_to="none"
)

data_collator = DataCollatorForLanguageModeling(tokenizer, mlm=False)

trainer = Trainer(
    model=model,
    args=training_args,
    train_dataset=tokenized_dataset["train"],
    eval_dataset=tokenized_dataset["test"],
    data_collator=data_collator
)

trainer.train()

def generate_response(prompt):
    input_text = f"{prompt}\nResponse:"
    input_ids = tokenizer(input_text, return_tensors="pt").input_ids

    output = model.generate(
        input_ids=input_ids,
        attention_mask=(input_ids != tokenizer.pad_token_id),
        generation_config=gen_config
    )

    decoded = tokenizer.decode(output[0], skip_special_tokens=True)
    print("\n📤 Raw decoded:", decoded)

    # Extract after 'Response:'
    response_raw = decoded.split("Response:")[-1].strip()

    # Clean: first full sentence starting with "Executing:"
    match = re.search(r'(Executing:.+?[.!?])', response_raw, re.IGNORECASE)
    response_clean = match.group(1).strip() if match else "[Unclear or no valid response]"

    # Normalize capitalization
    if response_clean.lower().startswith("executing:"):
        response_clean = "Executing:" + response_clean[len("executing:"):].strip()

    print("🚗 Model Response:", response_clean)

generate_response("Turn left after the red light and park near the blue building.")