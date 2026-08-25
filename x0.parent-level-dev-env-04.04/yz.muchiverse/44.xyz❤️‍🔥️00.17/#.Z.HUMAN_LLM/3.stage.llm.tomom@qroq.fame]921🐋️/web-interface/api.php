<?php
// api.php - PHP API endpoints for the chatbot interface

header("Content-Type: application/json");
header("Access-Control-Allow-Origin: *");
header("Access-Control-Allow-Methods: GET, POST, OPTIONS");
header("Access-Control-Allow-Headers: Content-Type");

// Handle preflight requests
if ($_SERVER["REQUEST_METHOD"] === "OPTIONS") {
    http_response_code(200);
    exit();
}

$request_method = $_SERVER["REQUEST_METHOD"];
$request_uri = parse_url($_SERVER["REQUEST_URI"], PHP_URL_PATH);

// Get the relative path by removing the script name
$script_dir = dirname($_SERVER["SCRIPT_NAME"]);
$relative_path = substr($request_uri, strlen($script_dir));

if ($request_method === "GET" && $relative_path === "/api/curricula") {
    get_curricula();
} elseif ($request_method === "POST" && $relative_path === "/api/chat") {
    run_chatbot();
} elseif ($request_method === "GET" && $relative_path === "/api/debug") {
    get_debug_info();
} else {
    http_response_code(404);
    echo json_encode(["error" => "Endpoint not found"]);
}

function get_curricula() {
    $files = array_filter(scandir("."), function($file) {
        return (pathinfo($file, PATHINFO_EXTENSION) === "txt" && 
                (strpos($file, "curriculum") !== false || strpos($file, "vocab") !== false)) ||
               (pathinfo($file, PATHINFO_EXTENSION) === "txt" && 
                strpos($file, "model") !== false);
    });
    
    echo json_encode(["files" => array_values($files)]);
}

function run_chatbot() {
    $input = json_decode(file_get_contents("php://input"), true);
    
    $prompt = $input["prompt"] ?? "";
    $curriculum = $input["curriculum"] ?? "";
    $responseLength = $input["responseLength"] ?? 10;
    $temperature = $input["temperature"] ?? 1.0;
    
    // Validate inputs
    if (empty($prompt) || empty($curriculum)) {
        http_response_code(400);
        echo json_encode(["error" => "Prompt and curriculum file are required"]);
        return;
    }
    
    // Validate parameters
    if ($responseLength < 1 || $responseLength > 100) {
        http_response_code(400);
        echo json_encode(["error" => "Response length must be between 1 and 100"]);
        return;
    }
    
    if ($temperature < 0.1 || $temperature > 10.0) {
        http_response_code(400);
        echo json_encode(["error" => "Temperature must be between 0.1 and 10.0"]);
        return;
    }
    
    // Validate that curriculum file exists
    if (!file_exists($curriculum)) {
        http_response_code(400);
        echo json_encode(["error" => "Curriculum file does not exist: $curriculum"]);
        return;
    }
    
    // Check if the chatbot executable exists in the +x directory
    $chatbotPath = "./+x/chatbot_moe_v1.+x";
    $command = "";
    
    if (file_exists($chatbotPath)) {
        $command = "./+x/chatbot_moe_v1.+x \"$curriculum\" \"$prompt\" $responseLength $temperature";
    } else {
        // Fallback to direct C file compilation and execution
        $command = "gcc -o temp_chatbot chatbot_moe_v1.c -lm && ./temp_chatbot \"$curriculum\" \"$prompt\" $responseLength $temperature";
    }
    
    // Execute the chatbot command
    $output = [];
    $return_var = 0;
    exec($command, $output, $return_var);
    
    // Clean up temporary executable if it was created
    if (!file_exists($chatbotPath)) {
        if (file_exists("./temp_chatbot")) {
            unlink("./temp_chatbot");
        }
    }
    
    if ($return_var !== 0) {
        http_response_code(500);
        echo json_encode(["error" => "Failed to execute chatbot. Return code: $return_var", "details" => implode("\n", $output)]);
        return;
    }
    
    $output_str = implode("\n", $output);
    
    // Extract response from output
    $response = $output_str;
    if (preg_match("/Response:\s*(.*)/", $output_str, $matches)) {
        $response = trim($matches[1]);
    }
    
    echo json_encode([
        "prompt" => $prompt,
        "response" => $response,
        "curriculum" => $curriculum,
        "length" => $responseLength,
        "temperature" => $temperature
    ]);
}

function get_debug_info() {
    $debug_content = "No debug information available";
    
    if (file_exists("debug_chain.txt")) {
        $debug_content = file_get_contents("debug_chain.txt");
    }
    
    echo json_encode(["debug" => $debug_content]);
}
?>
