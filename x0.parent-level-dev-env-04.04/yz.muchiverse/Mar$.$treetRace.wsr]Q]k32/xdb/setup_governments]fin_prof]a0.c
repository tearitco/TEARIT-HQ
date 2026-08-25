#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <ctype.h>

#define MAX_LINE_LEN 256
#define MAX_FILE_SIZE 1048576 // 1MB buffer for input file

// Structure to hold government data
typedef struct {
    char name[100];
    long long gdp;
    long long population;
} Government;

// Function to create a directory if it doesn't exist
void create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0700);
    }
}

// Function to trim leading and trailing whitespace
void trim_whitespace(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (start <= end && isspace((unsigned char)str[start])) start++;
    while (end >= start && isspace((unsigned char)str[end])) end--;
    str[end + 1] = '\0';
    memmove(str, str + start, end - start + 2);
}

// Function to sanitize name for filename
void sanitize_name(char *dest, const char *src) {
    int j = 0;
    for (int i = 0; src[i] && j < 99; i++) {
        if (isalnum((unsigned char)src[i]) || src[i] == ' ') {
            dest[j++] = src[i];
        } else {
            dest[j++] = '_';
        }
    }
    dest[j] = '\0';
    trim_whitespace(dest);
}

// Function to format a number with four decimal places
void format_number(char *dest, double num) {
    sprintf(dest, "%.4f", num);
}

// Function to read template file into a buffer
char *read_template(const char *template_path) {
    FILE *template_fp = fopen(template_path, "r");
    if (template_fp == NULL) {
        perror("Error opening template file");
        return NULL;
    }

    char *buffer = malloc(MAX_FILE_SIZE);
    if (buffer == NULL) {
        fclose(template_fp);
        return NULL;
    }

    size_t bytes_read = fread(buffer, 1, MAX_FILE_SIZE - 1, template_fp);
    buffer[bytes_read] = '\0';
    fclose(template_fp);
    return buffer;
}

// Function to generate and save government sheets
void generate_gov_sheet(Government gov) {
    char dir_path[256];
    char file_path[256];
    char balance_sheet_path[256];
    char financial_profile_path[256];
    char sanitized_name[100];

    sanitize_name(sanitized_name, gov.name);
    snprintf(dir_path, sizeof(dir_path), "governments/generated/%s", sanitized_name);

    // Create the government-specific directory
    create_directory(dir_path);

    // Construct file paths after directory is created
    snprintf(file_path, sizeof(file_path), "%s/%s.txt", dir_path, sanitized_name);
    snprintf(balance_sheet_path, sizeof(balance_sheet_path), "%s/balance_sheet.txt", dir_path);
    snprintf(financial_profile_path, sizeof(financial_profile_path), "%s/financial_profile.txt", dir_path);

    // Generate government data sheet
    FILE *fp = fopen(file_path, "w");
    if (fp == NULL) {
        perror("Error creating government sheet");
        return;
    }

    fprintf(fp, "GOVERNMENT FINANCIAL DATA: %s\n", gov.name);
    fprintf(fp, "---------------------------------------------\n");
    fprintf(fp, "GDP: %lld\n", gov.gdp);
    fprintf(fp, "Population: %lld\n", gov.population);

    fclose(fp);

    // Generate balance sheet
    FILE *bs_fp = fopen(balance_sheet_path, "w");
    if (bs_fp == NULL) {
        perror("Error creating balance sheet");
        return;
    }

    // Simplified balance sheet based on GDP
    double bs_cash = gov.gdp / 10.0; // Assume 10% of GDP as cash
    double bs_net_worth = bs_cash; // Simplified: no debt for now

    char cash_str[64], bs_net_worth_str[64];
    format_number(cash_str, bs_cash);
    format_number(bs_net_worth_str, bs_net_worth);

    fprintf(bs_fp, "My Balance Sheet: %s\n", gov.name);
    fprintf(bs_fp, "Cash (CD's)..\t\t%s\n", cash_str);
    fprintf(bs_fp, "Other Assets....\t\t0.0000\n");
    fprintf(bs_fp, "Total Assets.....\t\t%s\n", cash_str);
    fprintf(bs_fp, "Less Debt....\t\t-0.0000\n");
    fprintf(bs_fp, "Net Worth........\t\t%s\n", bs_net_worth_str);

    fclose(bs_fp);

    // Generate financial profile from template
    char *template = read_template("presets/financial_profile_template.txt");
    if (template == NULL) {
        return;
    }

    FILE *fp_fp = fopen(financial_profile_path, "w");
    if (fp_fp == NULL) {
        perror("Error creating financial profile");
        free(template);
        return;
    }

    // Calculate financial data based on raw GDP and population
    double cash_equivalents = gov.gdp * 0.0336; // 3.36% of GDP
    double tax_receivables = gov.gdp * 0.0123; // 1.23% of GDP
    double loans = gov.gdp * 0.0544; // 5.44% of GDP
    double physical_assets = gov.gdp * 0.0422; // 4.22% of GDP
    double investments = gov.gdp * 0.1655; // 16.55% of GDP
    double total_assets = cash_equivalents + tax_receivables + loans + physical_assets + investments;
    double public_debt = gov.gdp * 1.0467; // 104.67% of GDP
    double intragov_debt = gov.gdp * 0.3778; // 37.78% of GDP
    double pensions = gov.gdp * 0.2719; // 27.19% of GDP
    double env_liabilities = gov.gdp * 0.0123; // 1.23% of GDP
    double interest_payable = gov.gdp * 0.0081; // 0.81% of GDP
    double total_liabilities = public_debt + intragov_debt + pensions + env_liabilities + interest_payable;
    double net_worth = total_assets - total_liabilities;

    double total_revenue = gov.gdp * 0.1907; // 19.07% of GDP
    double individual_tax = gov.gdp * 0.1565; // 15.65% of GDP
    double corporate_tax = gov.gdp * 0.0205; // 2.05% of GDP
    double payroll_tax = gov.gdp * 0.0137; // 1.37% of GDP
    double net_cost = gov.gdp * 0.2835; // 28.35% of GDP
    double social_security = gov.gdp * 0.0560; // 5.60% of GDP
    double health = gov.gdp * 0.0380; // 3.80% of GDP
    double interest = gov.gdp * 0.0337; // 3.37% of GDP
    double life_support = gov.gdp * 0.0335; // 3.35% of GDP
    double defense = gov.gdp * 0.0335; // 3.35% of GDP
    double income_security = gov.gdp * 0.0257; // 2.57% of GDP
    double other_spending = gov.gdp * 0.0633; // 6.33% of GDP
    double net_operating = total_revenue - net_cost;
    double beginning_net_position = net_worth - net_operating;
    double debt_to_gdp = (public_debt + intragov_debt) / gov.gdp * 100.0;
    double deficit_to_spending = (-net_operating) / net_cost * 100.0;
    double interest_to_revenue = interest / total_revenue * 100.0;
    double tax_per_capita = total_revenue / gov.population;
    double spending_per_capita = net_cost / gov.population;

    // Format numbers with four decimal places
    char cash_eq_str[64], tax_rec_str[64], loans_str[64], phys_assets_str[64], inv_str[64], total_assets_str[64];
    char pub_debt_str[64], intra_debt_str[64], pensions_str[64], env_liab_str[64], int_pay_str[64], total_liab_str[64];
    char net_worth_str[64], total_rev_str[64], ind_tax_str[64], corp_tax_str[64], payroll_tax_str[64];
    char net_cost_str[64], soc_sec_str[64], health_str[64], interest_str[64], life_support_str[64], defense_str[64];
    char inc_sec_str[64], other_spend_str[64], net_op_str[64], beg_net_pos_str[64];
    char tax_per_capita_str[64], spending_per_capita_str[64], debt_to_gdp_str[64], def_to_spend_str[64], int_to_rev_str[64];

    format_number(cash_eq_str, cash_equivalents);
    format_number(tax_rec_str, tax_receivables);
    format_number(loans_str, loans);
    format_number(phys_assets_str, physical_assets);
    format_number(inv_str, investments);
    format_number(total_assets_str, total_assets);
    format_number(pub_debt_str, public_debt);
    format_number(intra_debt_str, intragov_debt);
    format_number(pensions_str, pensions);
    format_number(env_liab_str, env_liabilities);
    format_number(int_pay_str, interest_payable);
    format_number(total_liab_str, total_liabilities);
    format_number(net_worth_str, net_worth);
    format_number(total_rev_str, total_revenue);
    format_number(ind_tax_str, individual_tax);
    format_number(corp_tax_str, corporate_tax);
    format_number(payroll_tax_str, payroll_tax);
    format_number(net_cost_str, net_cost);
    format_number(soc_sec_str, social_security);
    format_number(health_str, health);
    format_number(interest_str, interest);
    format_number(life_support_str, life_support);
    format_number(defense_str, defense);
    format_number(inc_sec_str, income_security);
    format_number(other_spend_str, other_spending);
    format_number(net_op_str, net_operating);
    format_number(beg_net_pos_str, beginning_net_position);
    format_number(tax_per_capita_str, tax_per_capita);
    format_number(spending_per_capita_str, spending_per_capita);
    format_number(debt_to_gdp_str, debt_to_gdp);
    format_number(def_to_spend_str, deficit_to_spending);
    format_number(int_to_rev_str, interest_to_revenue);

    // Replace placeholders in template
    char *output = malloc(MAX_FILE_SIZE);
    if (output == NULL) {
        fclose(fp_fp);
        free(template);
        return;
    }
    strcpy(output, template);

    char *pos = output;
    char temp[MAX_FILE_SIZE];
    temp[0] = '\0';
    struct { const char *placeholder; const char *value; } replacements[] = {
        {"{{GOV_NAME}}", gov.name},
        {"{{CASH_EQUIVALENTS}}", cash_eq_str},
        {"{{TAX_RECEIVABLES}}", tax_rec_str},
        {"{{LOANS}}", loans_str},
        {"{{PHYSICAL_ASSETS}}", phys_assets_str},
        {"{{INVESTMENTS}}", inv_str},
        {"{{TOTAL_ASSETS}}", total_assets_str},
        {"{{PUBLIC_DEBT}}", pub_debt_str},
        {"{{INTRAGOV_DEBT}}", intra_debt_str},
        {"{{PENSIONS}}", pensions_str},
        {"{{ENV_LIABILITIES}}", env_liab_str},
        {"{{INTEREST_PAYABLE}}", int_pay_str},
        {"{{TOTAL_LIABILITIES}}", total_liab_str},
        {"{{NET_WORTH}}", net_worth_str},
        {"{{TOTAL_REVENUE}}", total_rev_str},
        {"{{INDIVIDUAL_TAX}}", ind_tax_str},
        {"{{CORPORATE_TAX}}", corp_tax_str},
        {"{{PAYROLL_TAX}}", payroll_tax_str},
        {"{{NET_COST}}", net_cost_str},
        {"{{SOCIAL_SECURITY}}", soc_sec_str},
        {"{{HEALTH}}", health_str},
        {"{{INTEREST}}", interest_str},
        {"{{LIFE_SUPPORT}}", life_support_str},
        {"{{DEFENSE}}", defense_str},
        {"{{INCOME_SECURITY}}", inc_sec_str},
        {"{{OTHER_SPENDING}}", other_spend_str},
        {"{{NET_OPERATING}}", net_op_str},
        {"{{BEGINNING_NET_POSITION}}", beg_net_pos_str},
        {"{{DEBT_TO_GDP}}", debt_to_gdp_str},
        {"{{DEFICIT_TO_SPENDING}}", def_to_spend_str},
        {"{{INTEREST_TO_REVENUE}}", int_to_rev_str},
        {"{{TAX_PER_CAPITA}}", tax_per_capita_str},
        {"{{SPENDING_PER_CAPITA}}", spending_per_capita_str},
        {"{{GDP}}", NULL}
    };

    for (int i = 0; i < 34; i++) {
        pos = output;
        temp[0] = '\0';
        char value_str[64];
        if (replacements[i].value) {
            strcpy(value_str, replacements[i].value);
        } else if (strcmp(replacements[i].placeholder, "{{GDP}}") == 0) {
            format_number(value_str, (double)gov.gdp);
        }
        while ((pos = strstr(pos, replacements[i].placeholder))) {
            strncpy(temp, output, pos - output);
            temp[pos - output] = '\0';
            strcat(temp, value_str);
            strcat(temp, pos + strlen(replacements[i].placeholder));
            strcpy(output, temp);
            pos += strlen(value_str);
        }
    }

    fprintf(fp_fp, "%s", output);
    fclose(fp_fp);
    free(template);
    free(output);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input_file>\n", argv[0]);
        return 1;
    }

    // Process preset_bank.txt to get humanoid_bank values
    FILE *preset_fp = fopen("presets/preset_bank.txt", "r");
    if (preset_fp == NULL) {
        perror("Error opening presets/preset_bank.txt");
        return 1;
    }

    create_directory("multiverse");
    create_directory("multiverse/humanoids");
    create_directory("presets");

    FILE *data_bank_fp = fopen("multiverse/humanoids/humanoid_data_bank.txt", "w");
    if (data_bank_fp == NULL) {
        perror("Error creating humanoid_data_bank.txt");
        fclose(preset_fp);
        return 1;
    }

    long long total_population = 0;
    long long total_gdp = 0;
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), preset_fp)) {
        char pool_name[100];
        long long pool_pop = 0;
        long long cash_per_cap = 0;

        // Parse line: name pop cash_per_cap
        char *token = strtok(line, " ");
        if (token) strncpy(pool_name, token, sizeof(pool_name) - 1);
        token = strtok(NULL, " ");
        if (token) pool_pop = atoll(token);
        token = strtok(NULL, " ");
        if (token) cash_per_cap = atoll(token);

        // Only use humanoid_bank for totals
        if (strcmp(pool_name, "humanoid_bank") == 0) {
            total_population = pool_pop;
            total_gdp = pool_pop * cash_per_cap;
        }

        long long pool_money = pool_pop * cash_per_cap;
        char money_str[64];
        format_number(money_str, (double)pool_money);
        fprintf(data_bank_fp, "%s %lld %s\n", pool_name, pool_pop, money_str);
    }

    fclose(preset_fp);
    fclose(data_bank_fp);

    // Process the input gov list
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        perror("Error opening input file");
        return 1;
    }

    system("rm -rf governments/generated/*");
    create_directory("governments/generated");

    FILE *out_fp = fopen("governments/generated/gov-list.txt", "w");
    if (out_fp == NULL) {
        perror("Error creating gov-list.txt");
        fclose(fp);
        return 1;
    }

    fprintf(out_fp, "Country pop gdp\n");

    // Process input file to generate individual government sheets
    while (fgets(line, sizeof(line), fp)) {
        Government gov = {0};
        char rank[100];
        char pop_perc[100];
        char gdp_share_perc[100];

        char *token;
        token = strtok(line, "\t"); // Rank
        if (token != NULL) {
            strncpy(rank, token, sizeof(rank) - 1);
            rank[sizeof(rank) - 1] = '\0';
            trim_whitespace(rank);
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // Country Name
        if (token != NULL) {
            strncpy(gov.name, token, sizeof(gov.name) - 1);
            gov.name[sizeof(gov.name) - 1] = '\0';
            trim_whitespace(gov.name);
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // pop%
        if (token != NULL) {
            strncpy(pop_perc, token, sizeof(pop_perc) - 1);
            trim_whitespace(pop_perc);
            int len = strlen(pop_perc);
            if (len > 0 && pop_perc[len - 1] == '%') pop_perc[len - 1] = '\0';
        } else {
            continue;
        }

        token = strtok(NULL, "\t"); // gdp.share%
        if (token != NULL) {
            strncpy(gdp_share_perc, token, sizeof(gdp_share_perc) - 1);
            trim_whitespace(gdp_share_perc);
            int len = strlen(gdp_share_perc);
            if (len > 0 && gdp_share_perc[len - 1] == '%') gdp_share_perc[len - 1] = '\0';
        } else {
            continue;
        }

        // Skip the last gdp 0
        strtok(NULL, "\t");

        double pop_frac = atof(pop_perc) / 100.0;
        double gdp_frac = atof(gdp_share_perc) / 100.0;

        gov.population = (long long)(pop_frac * total_population + 0.5);
        gov.gdp = (long long)(gdp_frac * total_gdp + 0.5);

        if (strlen(gov.name) > 0) {
            generate_gov_sheet(gov);
            fprintf(out_fp, "%s\t%s\t%lld %lld\n", rank, gov.name, gov.population, gov.gdp);
        }
    }

    fclose(fp);
    fclose(out_fp);

    return 0;
}
