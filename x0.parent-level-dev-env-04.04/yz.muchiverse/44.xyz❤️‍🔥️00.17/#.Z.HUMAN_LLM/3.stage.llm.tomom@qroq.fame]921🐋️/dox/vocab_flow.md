# Vocabulary Flow Through the Pipeline

This diagram shows how vocabulary data is created, processed, and used in the chatbot system.

## 1. Vocabulary Creation Flow

```
Input Text Files (corpuses/*.txt)
         ↓
    [vocab_model.c]
         ↓
   Tokenization & Processing
         ↓
    vocab_model.txt
[Word Index] [Word] [Embedding] [PE] [Weight] [Bias1] [Bias2] [Bias3] [Bias4]
     1      start-token   0.84    0.00   0.39     0.00    0.00    0.00    0.00
     2      emoji         0.78    0.04   0.80     0.00    0.00    0.00    0.00
     3      test          0.91    0.08   0.19     0.00    0.00    0.00    0.00
     4      file          0.34    0.13   0.77     0.00    0.00    0.00    0.00
     5      😀            0.28    0.17   0.55     0.00    0.00    0.00    0.00
     6      😃            0.48    0.21   0.63     0.00    0.00    0.00    0.00
     ...    ...           ...     ...    ...      ...     ...     ...     ...
```

## 2. Training Flow

```
vocab_model.txt → [trainer.c] → Updated Parameters
                      ↓
              Initializes Components
                      ↓
        [attention.c]  [mlp_layer.c]  [optimizer.c]
              ↓              ↓              ↓
    attention_model.txt  mlp_model.txt  optimizer_state.txt
              ↓              ↓              ↓
         Forward Pass Through Network ([forward_prop.c])
                      ↓
              Loss Computation & Backpropagation ([backward_prop.c])
                      ↓
              Parameter Updates (Adam Optimizer)
                      ↓
    All Model Files Updated (vocab_model.txt, attention_model.txt, etc.)
```

## 3. Prediction Flow

```
User Prompt → [chatbot.c] → Response
    ↓
Loads vocab_model.txt
    ↓
Tokenizes Input
    ↓
For Each Word:
  ┌─ Find word in vocabulary
  │
  ├─ Get word vector: [embedding, pe, weight, bias1, bias2, bias3, bias4]
  │
  ├─ Calculate dot product with all vocabulary words
  │
  ├─ Apply softmax with temperature sampling
  │
  └─ Select next word from top candidates
    ↓
Continue until "end-token" or length limit
    ↓
Generated Response
```

## Key Data Transformations

### Initial Vocabulary Entry:
```
Word: "😀"
Embedding: 0.28  (Random initial value)
PE: 0.17         (Positional encoding)
Weight: 0.55     (Random initial value)
Bias1-4: 0.00    (Zero-initialized)
```

### After Training:
```
Word: "😀"
Embedding: 0.42  (Learned value)
PE: 0.17         (Unchanged)
Weight: 0.68     (Learned value)
Bias1: 0.05      (Learned value)
Bias2: -0.02     (Learned value)
Bias3: 0.01      (Learned value)
Bias4: 0.03      (Learned value)
```

### During Prediction:
```
Input: "emoji 😀"
              ↓
Find "😀" in vocab_model.txt → Index 5
              ↓
Get word vector: [0.42, 0.17, 0.68, 0.05, -0.02, 0.01, 0.03]
              ↓
Calculate similarity with all words in vocabulary
              ↓
Apply softmax → Probability distribution
              ↓
Temperature sampling → Select next word
              ↓
Generate response
```