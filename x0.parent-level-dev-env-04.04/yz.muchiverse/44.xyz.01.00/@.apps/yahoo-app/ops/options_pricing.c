//file: options_pricing.c
// Compile: gcc -o ./+x/options_pricing.+x options_pricing.c -lm -D_POSIX_C_SOURCE=200809L
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#define MAX_LINE 256
#define EXPIRATIONS 5
#define PRICE_POINTS 5

// Expirations in years
const double expirations[EXPIRATIONS] = {1.0/8760, 1.0/365, 7.0/365, 1.0/12, 1.0};
const char *expiration_names[EXPIRATIONS] = {"1 hour", "1 day", "1 week", "1 month", "1 year"};
// Price change percentages: +2%, +4%, +8%, +16%, +32% for calls; -2%, -4%, -8%, -16%, -32% for puts
const double call_price_changes[PRICE_POINTS] = {0.02, 0.04, 0.08, 0.16, 0.32};
const double put_price_changes[PRICE_POINTS] = {-0.02, -0.04, -0.08, -0.16, -0.32};
const char *call_price_labels[PRICE_POINTS] = {"+2%", "+4%", "+8%", "+16%", "+32%"};
const char *put_price_labels[PRICE_POINTS] = {"-2%", "-4%", "-8%", "-16%", "-32%"};

// Normal CDF using erf
double norm_cdf(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}

// Black-Scholes option price (European)
void black_scholes(double S, double K, double T, double r, double sigma, double q,
                  double *call_price, double *put_price) {
    if (T <= 0 || sigma <= 0 || S <= 0 || K <= 0) {
        *call_price = 0.0;
        *put_price = 0.0;
        return;
    }
    double d1 = (log(S/K) + (r - q + sigma*sigma/2.0)*T) / (sigma*sqrt(T));
    double d2 = d1 - sigma*sqrt(T);
    *call_price = S * exp(-q*T) * norm_cdf(d1) - K * exp(-r*T) * norm_cdf(d2);
    *put_price = K * exp(-r*T) * norm_cdf(-d2) - S * exp(-q*T) * norm_cdf(-d1);
}

// Binomial model for American options
void binomial_adjust(double S, double K, double T, double r, double sigma, double q,
                    int steps, double *call_price, double *put_price) {
    if (T <= 0 || sigma <= 0 || S <= 0 || K <= 0) {
        *call_price = 0.0;
        *put_price = 0.0;
        return;
    }
    double dt = T / steps;
    double u = exp(sigma * sqrt(dt));
    double d = 1.0 / u;
    double p = (exp((r - q) * dt) - d) / (u - d);
    double discount = exp(-r * dt);

    double call_values[steps + 1];
    double put_values[steps + 1];

    // Initialize terminal payoffs
    for (int i = 0; i <= steps; i++) {
        double stock_price = S * pow(u, steps - i) * pow(d, i);
        call_values[i] = fmax(0, stock_price - K);
        put_values[i] = fmax(0, K - stock_price);
    }

    // Backward induction
    for (int j = steps - 1; j >= 0; j--) {
        for (int i = 0; i <= j; i++) {
            double stock_price = S * pow(u, j - i) * pow(d, i);
            call_values[i] = discount * (p * call_values[i] + (1 - p) * call_values[i + 1]);
            put_values[i] = discount * (p * put_values[i] + (1 - p) * put_values[i + 1]);
            // Early exercise
            call_values[i] = fmax(call_values[i], stock_price - K);
            put_values[i] = fmax(put_values[i], K - stock_price);
        }
    }

    *call_price = call_values[0];
    *put_price = put_values[0];
}

// Calculate option prices and P/L
void calculate_option_prices(const char *symbol, double S, double K, double r, double sigma, double q,
                            double call_prices[EXPIRATIONS], double put_prices[EXPIRATIONS],
                            double call_pl[EXPIRATIONS][PRICE_POINTS], double put_pl[EXPIRATIONS][PRICE_POINTS]) {
    for (int i = 0; i < EXPIRATIONS; i++) {
        double T = expirations[i];
        black_scholes(S, K, T, r, sigma, q, &call_prices[i], &put_prices[i]);
        binomial_adjust(S, K, T, r, sigma, q, 100, &call_prices[i], &put_prices[i]);
        
        // Calculate P/L for future prices
        for (int j = 0; j < PRICE_POINTS; j++) {
            double call_future_S = S * (1.0 + call_price_changes[j]);
            double put_future_S = S * (1.0 + put_price_changes[j]);
            if (call_future_S < 0) call_future_S = 0;
            if (put_future_S < 0) put_future_S = 0;
            call_pl[i][j] = (fmax(0, call_future_S - K) - call_prices[i]) * 100;
            put_pl[i][j] = (fmax(0, K - put_future_S) - put_prices[i]) * 100;
        }
    }
}

// Write options to CSV
void write_options_csv(const char *symbol, double K, double call_prices[EXPIRATIONS],
                       double put_prices[EXPIRATIONS]) {
    char filename[64];
    snprintf(filename, sizeof(filename), "option_prices.%s.csv", symbol);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[%s] Failed to open %s: %s\n", symbol, filename, strerror(errno));
        return;
    }

    // CSV header
    fprintf(fp, "index,type,expiration,strike,price\n");

    // Call options
    for (int i = 0; i < EXPIRATIONS; i++) {
        fprintf(fp, "%d,Call,%s,%.2f,%.2f\n", i+1, expiration_names[i], K, call_prices[i]);
    }

    // Put options
    for (int i = 0; i < EXPIRATIONS; i++) {
        fprintf(fp, "%d,Put,%s,%.2f,%.2f\n", i+6, expiration_names[i], K, put_prices[i]);
    }

    fclose(fp);
    fprintf(stderr, "[%s] Wrote CSV to %s\n", symbol, filename);
}

// Run test with dummy data
void run_test() {
    printf("Running options pricing test with dummy data...\n");
    char symbol[] = "TEST";
    double S = 100.0, K = 100.0, r = 0.05, sigma = 0.2, q = 0.01;
    double call_prices[EXPIRATIONS], put_prices[EXPIRATIONS];
    double call_pl[EXPIRATIONS][PRICE_POINTS], put_pl[EXPIRATIONS][PRICE_POINTS];
    calculate_option_prices(symbol, S, K, r, sigma, q, call_prices, put_prices, call_pl, put_pl);
    write_options_csv(symbol, K, call_prices, put_prices);
    
    // Verify against known Black-Scholes values
    double expected_call_1y = 10.45, expected_put_1y = 5.57;
    printf("\nTest KPIs:\n");
    printf("1-year Call Price: %.2f (Expected: %.2f, Error: %.2f%%)\n",
           call_prices[4], expected_call_1y, fabs(call_prices[4] - expected_call_1y) / expected_call_1y * 100);
    printf("1-year Put Price: %.2f (Expected: %.2f, Error: %.2f%%)\n",
           put_prices[4], expected_put_1y, fabs(put_prices[4] - expected_put_1y) / expected_put_1y * 100);
}

int main(int argc, char *argv[]) {
    if (argc == 2 && strcmp(argv[1], "-test") == 0) {
        run_test();
        return 0;
    }

    if (argc != 15) {
        fprintf(stderr, "Usage: %s -s <symbol> -p <stock_price> -k <strike> -r <risk_free_rate> -v <volatility> -d <dividend_yield> -t <current_time>\n", argv[0]);
        return 1;
    }

    char symbol[MAX_LINE];
    double S = 0, K = 0, r = 0, sigma = 0, q = 0;
    char current_time[32];

    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "-s") == 0) strncpy(symbol, argv[i+1], MAX_LINE-1);
        else if (strcmp(argv[i], "-p") == 0) S = atof(argv[i+1]);
        else if (strcmp(argv[i], "-k") == 0) K = atof(argv[i+1]);
        else if (strcmp(argv[i], "-r") == 0) r = atof(argv[i+1]);
        else if (strcmp(argv[i], "-v") == 0) sigma = atof(argv[i+1]);
        else if (strcmp(argv[i], "-d") == 0) q = atof(argv[i+1]);
        else if (strcmp(argv[i], "-t") == 0) strncpy(current_time, argv[i+1], 31);
    }

    if (S <= 0 || K <= 0 || sigma <= 0) {
        fprintf(stderr, "Invalid inputs: stock price, strike, and volatility must be positive\n");
        return 1;
    }

    double call_prices[EXPIRATIONS], put_prices[EXPIRATIONS];
    double call_pl[EXPIRATIONS][PRICE_POINTS], put_pl[EXPIRATIONS][PRICE_POINTS];
    calculate_option_prices(symbol, S, K, r, sigma, q, call_prices, put_prices, call_pl, put_pl);
    write_options_csv(symbol, K, call_prices, put_prices);

    return 0;
}

