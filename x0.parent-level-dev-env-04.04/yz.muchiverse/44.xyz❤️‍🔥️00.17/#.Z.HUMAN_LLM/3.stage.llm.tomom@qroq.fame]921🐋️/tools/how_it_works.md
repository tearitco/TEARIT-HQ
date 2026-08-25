# How It Works

## export_associations.c

This tool exports word associations from the vocabulary model to analyze what the model has learned.

### How It Works

1. **Reading Vocabulary**: The tool reads the `vocab_model.txt` file which contains:
   - Word identifiers
   - Actual words
   - 7-dimensional vectors for each word (embedding, positional encoding, weight, and 4 biases)

2. **Computing Associations**: For each pair of words, it computes cosine similarity between their 7-dimensional vectors:
   - Normalizes both vectors to unit length
   - Computes dot product (which equals cosine similarity for unit vectors)
   - Results in a value between -1 and 1, where:
     - 1.0 = identical vectors
     - 0.0 = orthogonal vectors
     - -1.0 = opposite vectors

3. **Output Formats**:
   - `associations.csv`: Full association matrix in CSV format
   - `associations_stats.txt`: Summary statistics

## visualize_associations.c

This tool provides an interactive OpenGL visualization of word associations.

### How It Works

1. **Physics Simulation**: Uses a force-directed graph layout algorithm:
   - Spring forces pull connected words together
   - Repulsive forces push all words apart
   - Damping simulates friction to stabilize the system

2. **Visual Representation**:
   - Nodes represent words (blue circles)
   - Edges represent strong associations (colored lines)
   - Text labels show word names
   - Color intensity indicates association strength

3. **Interaction Controls**:
   - Left mouse drag: Pan the view
   - Right mouse click: Reset view
   - Scroll wheel: Zoom in/out
   - R key: Reset node positions
   - ESC key: Quit

### Features

1. **Constant Text Size**: Text labels maintain consistent size during zooming
2. **Emoji Support**: Uses Noto Color Emoji font for emoji rendering
3. **Performance Optimized**: Limits to 50 nodes for smooth interaction
4. **Color Coding**: Edge colors indicate association strength

## Interpreting the Data

### From export_associations.c

1. **CSV Format**:
   - First row: Header with all words
   - First column: Words (row labels)
   - Cell [i,j]: Cosine similarity between word i and word j
   - Diagonal: Always 1.0 (word similarity with itself)

2. **High Association Values**:
   - Values close to 1.0 indicate strong association
   - Words that appeared together in training or have similar contexts

3. **Low Association Values**:
   - Values close to 0 or negative indicate weak/no association
   - Unrelated words or words with different contexts

### From visualize_associations.c

1. **Cluster Formation**: Groups of closely connected nodes indicate semantic clusters
2. **Edge Density**: Areas with many connections show strong semantic relationships
3. **Node Positioning**: Words with similar meanings tend to cluster together
4. **Edge Colors**: 
   - Red: Strong associations
   - Blue: Weak associations
   - Green: Medium associations

### Example Usage

```bash
# Export associations
./export_associations ../vocab_model.txt associations

# Visualize associations
./visualize_associations associations.csv
```

This creates:
- `associations.csv`: 88x88 matrix of word associations
- `associations_stats.txt`: Summary statistics

### What to Look For

1. **Semantic Clusters**: Groups of words with high mutual associations
2. **Similarity Patterns**: Words that should be related based on training data
3. **Unexpected Associations**: Interesting or surprising connections the model learned
4. **Training Effectiveness**: Whether related words have high associations

The association matrix and visualization can be used to:
- Debug model training
- Understand learned representations
- Identify potential issues in the model
- Visualize word relationships
- Analyze semantic clustering