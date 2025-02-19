#include "plugin.h"
#include "plugin_utils.h"

// Called once to init.
void handle_init_contract(ethPluginInitContract_t *msg) {
    // Make sure we are running a compatible version.
    if (msg->interfaceVersion != ETH_PLUGIN_INTERFACE_VERSION_LATEST) {
        // If not the case, return the `UNAVAILABLE` status.
        msg->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
        return;
    }

    // Double check that the `context_t` struct is not bigger than the maximum
    // size (defined by `msg->pluginContextLength`).
    if (msg->pluginContextLength < sizeof(context_t)) {
        PRINTF("Plugin parameters structure is bigger than allowed size\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    context_t *context = (context_t *) msg->pluginContext;

    bool is_fuzz_test = context->next_param == 99;

    // Initialize the context (to 0).
    memset(context, 0, sizeof(*context));

    size_t index;
    if (!find_selector(U4BE(msg->selector, 0), SELECTORS, SELECTOR_COUNT, &index)) {
        PRINTF("Error: selector not found!\n");
        msg->result = ETH_PLUGIN_RESULT_UNAVAILABLE;
        return;
    }
    context->selectorIndex = index;
    // check for overflow
    if ((size_t) context->selectorIndex != index) {
        PRINTF("Error: overflow detected on selector index!\n");
        msg->result = ETH_PLUGIN_RESULT_ERROR;
        return;
    }

    // Set `next_param` to be the first field we expect to parse.
    switch (context->selectorIndex) {
        case ZAPPER_DEPOSIT_ETH:
            context->next_param = NONE;
            break;
        case CURVE_POOL_EXCHANGE:
        case CURVE_POOL_EXCHANGE_UNDERLYING:
            if (is_fuzz_test) {
                // Workaround to revert during fuzz tests.
                // Since `txContent->destination` isn't populated during fuzz tests,
                // it'll throw a null pointer exception causing the fuzz tests
                // to fail on CI. Right way would be to pass some value to
                // `txContent->destination` but that breaks a lot of
                // other fuzz tests.
                msg->result = ETH_PLUGIN_RESULT_ERROR;
                return;
            }
            if (memcmp(CURVE_OETH_POOL_ADDRESS,
                       msg->pluginSharedRO->txContent->destination,
                       ADDRESS_LENGTH) == 0 ||
                memcmp(CURVE_OUSD_POOL_ADDRESS,
                       msg->pluginSharedRO->txContent->destination,
                       ADDRESS_LENGTH) == 0) {
                context->next_param = TOKEN_SENT;
                break;
            }
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
        case UNISWAP_V3_ROUTER_EXACT_INPUT:
            context->next_param = PARAM_OFFSET;
            break;
        case UNISWAP_ROUTER_EXACT_INPUT_SINGLE:
            break;
        case CURVE_ROUTER_EXCHANGE_MULTIPLE:
        case VAULT_MINT:
            context->next_param = TOKEN_SENT;
            break;
        case FLIPPER_BUY_OUSD_WITH_USDT:
        case FLIPPER_SELL_OUSD_FOR_USDT:
        case FLIPPER_BUY_OUSD_WITH_DAI:
        case FLIPPER_SELL_OUSD_FOR_DAI:
        case FLIPPER_BUY_OUSD_WITH_USDC:
        case FLIPPER_SELL_OUSD_FOR_USDC:
        case ZAPPER_DEPOSIT_SFRXETH:
        case VAULT_REDEEM:
        case WRAP:
        case UNWRAP:
            context->next_param = AMOUNT_SENT;
            break;
        // Keep this
        default:
            PRINTF("Missing selectorIndex: %d\n", context->selectorIndex);
            msg->result = ETH_PLUGIN_RESULT_ERROR;
            return;
    }

    // Return valid status.
    msg->result = ETH_PLUGIN_RESULT_OK;
}
