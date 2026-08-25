<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>3-Stage LLM Chatbot Interface</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <link rel="stylesheet" href="css/style.css">
</head>
<body>
    <div class="container mt-5">
        <div class="row">
            <div class="col-12">
                <h1 class="text-center mb-4">3-Stage LLM Chatbot Interface</h1>
                <p class="text-center text-muted">Powered by C-based neural network with Mixture of Experts (MOE)</p>
            </div>
        </div>

        <div class="row">
            <div class="col-md-8 mx-auto">
                <!-- Configuration Panel -->
                <div class="card mb-4">
                    <div class="card-header">
                        <h5>Configuration</h5>
                    </div>
                    <div class="card-body">
                        <form id="configForm">
                            <div class="mb-3">
                                <label for="curriculumFile" class="form-label">Curriculum File:</label>
                                <select class="form-select" id="curriculumFile" required>
                                    <option value="">Loading curriculum files...</option>
                                </select>
                                <div class="form-text">Select a vocabulary or curriculum file for the chatbot</div>
                            </div>
                            
                            <div class="row">
                                <div class="col-md-6">
                                    <div class="mb-3">
                                        <label for="responseLength" class="form-label">Response Length:</label>
                                        <input type="number" class="form-control" id="responseLength" value="10" min="1" max="100">
                                        <div class="form-text">Number of tokens to generate (1-100)</div>
                                    </div>
                                </div>
                                <div class="col-md-6">
                                    <div class="mb-3">
                                        <label for="temperature" class="form-label">Temperature:</label>
                                        <input type="number" class="form-control" id="temperature" value="1.0" min="0.1" max="10" step="0.1">
                                        <div class="form-text">Sampling temperature (0.1-10.0)</div>
                                    </div>
                                </div>
                            </div>
                        </form>
                    </div>
                </div>

                <!-- Chat Interface -->
                <div class="card mb-4">
                    <div class="card-header">
                        <h5>Chat Interface</h5>
                    </div>
                    <div class="card-body">
                        <div class="mb-3">
                            <label for="promptInput" class="form-label">Enter your prompt:</label>
                            <textarea class="form-control" id="promptInput" rows="3" placeholder="Type your message here..."></textarea>
                        </div>
                        <button id="sendBtn" class="btn btn-primary w-100">Generate Response</button>
                        <button id="clearBtn" class="btn btn-secondary w-100 mt-2">Clear Chat</button>
                    </div>
                </div>

                <!-- Response Display -->
                <div class="card mb-4">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h5>Response</h5>
                        <button id="debugBtn" class="btn btn-sm btn-outline-info">Show Debug</button>
                    </div>
                    <div class="card-body">
                        <div id="responseContainer" class="border rounded p-3 bg-light">
                            <p class="text-muted">Response will appear here...</p>
                            <div id="responseText" class="fw-bold"></div>
                        </div>
                    </div>
                </div>

                <!-- Debug Information -->
                <div class="card mb-4" id="debugCard" style="display: none;">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h5>Debug Information</h5>
                        <button id="hideDebugBtn" class="btn btn-sm btn-outline-secondary">Hide Debug</button>
                    </div>
                    <div class="card-body">
                        <div id="debugContainer" class="border rounded p-3 bg-light">
                            <pre id="debugText" class="text-muted">Debug information will appear here...</pre>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <!-- Loading Modal -->
    <div class="modal fade" id="loadingModal" tabindex="-1" aria-hidden="true">
        <div class="modal-dialog modal-dialog-centered">
            <div class="modal-content">
                <div class="modal-header">
                    <h5 class="modal-title">Processing</h5>
                </div>
                <div class="modal-body text-center">
                    <div class="spinner-border text-primary" role="status">
                        <span class="visually-hidden">Loading...</span>
                    </div>
                    <p class="mt-2">Generating response, please wait...</p>
                    <div class="progress mt-3">
                        <div id="progressBar" class="progress-bar progress-bar-striped progress-bar-animated" role="progressbar" style="width: 0%"></div>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js"></script>
    <script src="js/app.js"></script>
</body>
</html>
