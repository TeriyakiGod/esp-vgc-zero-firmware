#include "fs.h"
#include "display.h"
#include "bitsybox/bitsybox.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"

static const char *TAG = "VGC";

// #define NUM_OF_SPIN_TASKS   6
// #define SPIN_ITER           500000  //Actual CPU cycles used will depend on compiler optimization
// #define BITSY_ENGINE_TASK_PRIO      6
// #define STATS_TASK_PRIO     3
// #define STATS_TICKS         pdMS_TO_TICKS(1000)
// #define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE

// /**
//  * @brief   Function to print the CPU usage of tasks over a given duration.
//  *
//  * This function will measure and print the CPU usage of tasks over a specified
//  * number of ticks (i.e. real time stats). This is implemented by simply calling
//  * uxTaskGetSystemState() twice separated by a delay, then calculating the
//  * differences of task run times before and after the delay.
//  *
//  * @note    If any tasks are added or removed during the delay, the stats of
//  *          those tasks will not be printed.
//  * @note    This function should be called from a high priority task to minimize
//  *          inaccuracies with delays.
//  * @note    When running in dual core mode, each core will correspond to 50% of
//  *          the run time.
//  *
//  * @param   xTicksToWait    Period of stats measurement
//  *
//  * @return
//  *  - ESP_OK                Success
//  *  - ESP_ERR_NO_MEM        Insufficient memory to allocated internal arrays
//  *  - ESP_ERR_INVALID_SIZE  Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET
//  *  - ESP_ERR_INVALID_STATE Delay duration too short
//  */
// static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
// {
//     TaskStatus_t *start_array = NULL, *end_array = NULL;
//     UBaseType_t start_array_size, end_array_size;
//     configRUN_TIME_COUNTER_TYPE start_run_time, end_run_time;
//     esp_err_t ret;

//     //Allocate array to store current task states
//     start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
//     start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
//     if (start_array == NULL) {
//         ret = ESP_ERR_NO_MEM;
//         goto exit;
//     }
//     //Get current task states
//     start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
//     if (start_array_size == 0) {
//         ret = ESP_ERR_INVALID_SIZE;
//         goto exit;
//     }

//     vTaskDelay(xTicksToWait);

//     //Allocate array to store tasks states post delay
//     end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
//     end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
//     if (end_array == NULL) {
//         ret = ESP_ERR_NO_MEM;
//         goto exit;
//     }
//     //Get post delay task states
//     end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
//     if (end_array_size == 0) {
//         ret = ESP_ERR_INVALID_SIZE;
//         goto exit;
//     }

//     //Calculate total_elapsed_time in units of run time stats clock period.
//     uint32_t total_elapsed_time = (end_run_time - start_run_time);
//     if (total_elapsed_time == 0) {
//         ret = ESP_ERR_INVALID_STATE;
//         goto exit;
//     }

//     printf("| Task | Run Time | Percentage\n");
//     //Match each task in start_array to those in the end_array
//     for (int i = 0; i < start_array_size; i++) {
//         int k = -1;
//         for (int j = 0; j < end_array_size; j++) {
//             if (start_array[i].xHandle == end_array[j].xHandle) {
//                 k = j;
//                 //Mark that task have been matched by overwriting their handles
//                 start_array[i].xHandle = NULL;
//                 end_array[j].xHandle = NULL;
//                 break;
//             }
//         }
//         //Check if matching task found
//         if (k >= 0) {
//             uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
//             uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * CONFIG_FREERTOS_NUMBER_OF_CORES);
//             printf("| %s | %"PRIu32" | %"PRIu32"%%\n", start_array[i].pcTaskName, task_elapsed_time, percentage_time);
//         }
//     }

//     //Print unmatched tasks
//     for (int i = 0; i < start_array_size; i++) {
//         if (start_array[i].xHandle != NULL) {
//             printf("| %s | Deleted\n", start_array[i].pcTaskName);
//         }
//     }
//     for (int i = 0; i < end_array_size; i++) {
//         if (end_array[i].xHandle != NULL) {
//             printf("| %s | Created\n", end_array[i].pcTaskName);
//         }
//     }
//     ret = ESP_OK;

// exit:    //Common return path
//     free(start_array);
//     free(end_array);
//     return ret;
// }

// static void stats_task(void *arg)
// {
//     //Print real time stats periodically
//     while (1) {
//         if (print_real_time_stats(STATS_TICKS) != ESP_OK) {
//             printf("Error getting real time stats\n");
//         }
//         vTaskDelay(pdMS_TO_TICKS(1000));
//     }
// }

void app_main(void)
{
    //Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_LOGI(TAG, "Starting BitsyBox");
    // Init FS
    ESP_ERROR_CHECK(vgc_fs_init());
    /* LCD HW initialization */
    ESP_ERROR_CHECK(vgc_lcd_init());
    // Start BitsyBox
    xTaskCreatePinnedToCore(vBitsyEngineTask, "BitsyEngineTask", 12 * 1024, NULL, 6, NULL, 1);
    //Create and start stats task
    //xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, STATS_TASK_PRIO, NULL, tskNO_AFFINITY);
}