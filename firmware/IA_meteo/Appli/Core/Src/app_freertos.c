/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.c
  * Description        : FreeRTOS applicative file
  *                      TP2 partie 3 - étape 1 : deux tâches
  *                        - TacheCapteurs : lecture des capteurs toutes les 5 s
  *                        - TacheLed      : LED verte qui clignote (signe de vie)
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "main.h"
#include "led.h"
#include "sensors.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
FreeMarker template error (DEBUG mode; use RETHROW in production!):
The following has evaluated to null or missing:
==> RTOS.tasks [in template "freertos_typedefs_cmsis_v2.ftl" at line 9, column 12]
----
Tip: It's the step after the last dot that caused this error, not those before it.
----
Tip: If the failing expression is known to legally refer to something that's sometimes null or missing, either specify a default value like myOptionalVar!myDefault, or use [#if myOptionalVar??]when-present[#else]when-missing[/#if]. (These only cover the last step of the expression; to cover the whole expression, use parenthesis: (myOptionalVar.foo)!myDefault, (myOptionalVar.foo)??
----
----
FTL stack trace ("~" means nesting-related):
- Failed at: #list RTOS.tasks as task [in template "freertos_typedefs_cmsis_v2.ftl" at line 9, column 5]
----
Java stack trace (for programmers):
----
freemarker.core.InvalidReferenceException: [... Exception message was already printed; see it above ...]
at freemarker.core.InvalidReferenceException.getInstance(InvalidReferenceException.java:134)
at freemarker.core.Expression.assertNonNull(Expression.java:249)
at freemarker.core.IteratorBlock.acceptWithResult(IteratorBlock.java:104)
at freemarker.core.IteratorBlock.accept(IteratorBlock.java:94)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.visit(Environment.java:376)
at freemarker.core.Environment.visitAndTransform(Environment.java:501)
at freemarker.core.CompressedBlock.accept(CompressedBlock.java:42)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.process(Environment.java:313)
at freemarker.template.Template.process(Template.java:383)
at com.st.microxplorer.codegenerator.CodeEngine.freemarkerDo(CodeEngine.java:420)
at com.st.microxplorer.codegenerator.CodeEngine.genCode(CodeEngine.java:273)
at com.st.microxplorer.codegenerator.CodeGenerator.generateOutputCode(CodeGenerator.java:6459)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCode(CodeGenerator.java:5281)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCodeFile(CodeGenerator.java:1862)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCodeFiles(CodeGenerator.java:2188)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCode(CodeGenerator.java:1691)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.generateCode(ProjectBuilder.java:3343)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createCode(ProjectBuilder.java:2235)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:819)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:608)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager.startGenerateCode(MainProjectManager.java:1497)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager$3.onCommandEntered(MainProjectManager.java:283)
at com.st.components.util.CommandLineManager$3.run(CommandLineManager.java:175)
at java.desktop/java.awt.event.InvocationEvent.dispatch(InvocationEvent.java:308)
at java.desktop/java.awt.EventQueue.dispatchEventImpl(EventQueue.java:773)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:720)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:714)
at java.base/java.security.AccessController.doPrivileged(AccessController.java:400)
at java.base/java.security.ProtectionDomain$JavaSecurityAccessImpl.doIntersectionPrivilege(ProtectionDomain.java:87)
at java.desktop/java.awt.EventQueue.dispatchEvent(EventQueue.java:742)
at java.desktop/java.awt.EventDispatchThread.pumpOneEventForFilters(EventDispatchThread.java:203)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForFilter(EventDispatchThread.java:124)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForHierarchy(EventDispatchThread.java:113)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:109)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:101)
at java.desktop/java.awt.EventDispatchThread.run(EventDispatchThread.java:90)
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SENSOR_PERIOD_MS   5000U   /* une lecture des capteurs toutes les 5 s   */
#define LED_BUSY_MIN_MS    300U    /* LED rouge allumée au moins 300 ms (visible) */
#define LED_BLINK_MS       500U    /* demi-période du clignotement de la LED verte */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* hi2c1 est créé par CubeMX dans main.c : "extern" dit au compilateur
 * qu'il existe ailleurs, sans le recréer. */
extern I2C_HandleTypeDef hi2c1;

/* --- TacheCapteurs ---------------------------------------------------------
 * Priorité Normal : c'est elle qui fait le travail utile.
 * Pile 1024 mots = 4096 octets : printf + snprintf consomment beaucoup de pile.
 * ATTENTION : en CMSIS-RTOS v2, .stack_size est en OCTETS (d'où le "* 4"). */
osThreadId_t TacheCapteursHandle;
const osThreadAttr_t TacheCapteurs_attributes = {
  .name       = "TacheCapteurs",
  .priority   = (osPriority_t) osPriorityNormal,
  .stack_size = 1024 * 4
};

/* --- TacheLed --------------------------------------------------------------
 * Priorité Low : elle ne fait presque rien, elle passe après les autres.
 * Pile 256 mots = 1024 octets : suffisant, elle n'appelle pas printf. */
osThreadId_t TacheLedHandle;
const osThreadAttr_t TacheLed_attributes = {
  .name       = "TacheLed",
  .priority   = (osPriority_t) osPriorityLow,
  .stack_size = 256 * 4
};

static sensors_data_t s_data;

/* USER CODE END Variables */
FreeMarker template error (DEBUG mode; use RETHROW in production!):
The following has evaluated to null or missing:
==> RTOS.tasks [in template "freertos_vars_cmsis_v2.ftl" at line 9, column 12]
----
Tip: It's the step after the last dot that caused this error, not those before it.
----
Tip: If the failing expression is known to legally refer to something that's sometimes null or missing, either specify a default value like myOptionalVar!myDefault, or use [#if myOptionalVar??]when-present[#else]when-missing[/#if]. (These only cover the last step of the expression; to cover the whole expression, use parenthesis: (myOptionalVar.foo)!myDefault, (myOptionalVar.foo)??
----
----
FTL stack trace ("~" means nesting-related):
- Failed at: #list RTOS.tasks as task [in template "freertos_vars_cmsis_v2.ftl" at line 9, column 5]
----
Java stack trace (for programmers):
----
freemarker.core.InvalidReferenceException: [... Exception message was already printed; see it above ...]
at freemarker.core.InvalidReferenceException.getInstance(InvalidReferenceException.java:134)
at freemarker.core.Expression.assertNonNull(Expression.java:249)
at freemarker.core.IteratorBlock.acceptWithResult(IteratorBlock.java:104)
at freemarker.core.IteratorBlock.accept(IteratorBlock.java:94)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.visit(Environment.java:376)
at freemarker.core.Environment.visitAndTransform(Environment.java:501)
at freemarker.core.CompressedBlock.accept(CompressedBlock.java:42)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.process(Environment.java:313)
at freemarker.template.Template.process(Template.java:383)
at com.st.microxplorer.codegenerator.CodeEngine.freemarkerDo(CodeEngine.java:420)
at com.st.microxplorer.codegenerator.CodeEngine.genCode(CodeEngine.java:273)
at com.st.microxplorer.codegenerator.CodeGenerator.generateOutputCode(CodeGenerator.java:6459)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCode(CodeGenerator.java:5281)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCodeFile(CodeGenerator.java:1862)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCodeFiles(CodeGenerator.java:2188)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCode(CodeGenerator.java:1691)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.generateCode(ProjectBuilder.java:3343)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createCode(ProjectBuilder.java:2235)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:819)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:608)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager.startGenerateCode(MainProjectManager.java:1497)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager$3.onCommandEntered(MainProjectManager.java:283)
at com.st.components.util.CommandLineManager$3.run(CommandLineManager.java:175)
at java.desktop/java.awt.event.InvocationEvent.dispatch(InvocationEvent.java:308)
at java.desktop/java.awt.EventQueue.dispatchEventImpl(EventQueue.java:773)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:720)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:714)
at java.base/java.security.AccessController.doPrivileged(AccessController.java:400)
at java.base/java.security.ProtectionDomain$JavaSecurityAccessImpl.doIntersectionPrivilege(ProtectionDomain.java:87)
at java.desktop/java.awt.EventQueue.dispatchEvent(EventQueue.java:742)
at java.desktop/java.awt.EventDispatchThread.pumpOneEventForFilters(EventDispatchThread.java:203)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForFilter(EventDispatchThread.java:124)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForHierarchy(EventDispatchThread.java:113)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:109)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:101)
at java.desktop/java.awt.EventDispatchThread.run(EventDispatchThread.java:90)

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
void TacheCapteurs(void *argument);
void TacheLed(void *argument);
static void log_etat(const char *etat);
/* USER CODE END FunctionPrototypes */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
FreeMarker template error (DEBUG mode; use RETHROW in production!):
The following has evaluated to null or missing:
==> RTOS.mutexes [in template "freertos_init_c_cmsis_v2.ftl" at line 7, column 12]
----
Tip: It's the step after the last dot that caused this error, not those before it.
----
Tip: If the failing expression is known to legally refer to something that's sometimes null or missing, either specify a default value like myOptionalVar!myDefault, or use [#if myOptionalVar??]when-present[#else]when-missing[/#if]. (These only cover the last step of the expression; to cover the whole expression, use parenthesis: (myOptionalVar.foo)!myDefault, (myOptionalVar.foo)??
----
----
FTL stack trace ("~" means nesting-related):
- Failed at: #list RTOS.mutexes as mutex [in template "freertos_init_c_cmsis_v2.ftl" at line 7, column 5]
----
Java stack trace (for programmers):
----
freemarker.core.InvalidReferenceException: [... Exception message was already printed; see it above ...]
at freemarker.core.InvalidReferenceException.getInstance(InvalidReferenceException.java:134)
at freemarker.core.Expression.assertNonNull(Expression.java:249)
at freemarker.core.IteratorBlock.acceptWithResult(IteratorBlock.java:104)
at freemarker.core.IteratorBlock.accept(IteratorBlock.java:94)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.visit(Environment.java:376)
at freemarker.core.Environment.visitAndTransform(Environment.java:501)
at freemarker.core.CompressedBlock.accept(CompressedBlock.java:42)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.process(Environment.java:313)
at freemarker.template.Template.process(Template.java:383)
at com.st.microxplorer.codegenerator.CodeEngine.freemarkerDo(CodeEngine.java:420)
at com.st.microxplorer.codegenerator.CodeEngine.genCode(CodeEngine.java:273)
at com.st.microxplorer.codegenerator.CodeGenerator.generateOutputCode(CodeGenerator.java:6459)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCode(CodeGenerator.java:5281)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCodeFile(CodeGenerator.java:1862)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCodeFiles(CodeGenerator.java:2188)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCode(CodeGenerator.java:1691)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.generateCode(ProjectBuilder.java:3343)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createCode(ProjectBuilder.java:2235)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:819)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:608)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager.startGenerateCode(MainProjectManager.java:1497)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager$3.onCommandEntered(MainProjectManager.java:283)
at com.st.components.util.CommandLineManager$3.run(CommandLineManager.java:175)
at java.desktop/java.awt.event.InvocationEvent.dispatch(InvocationEvent.java:308)
at java.desktop/java.awt.EventQueue.dispatchEventImpl(EventQueue.java:773)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:720)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:714)
at java.base/java.security.AccessController.doPrivileged(AccessController.java:400)
at java.base/java.security.ProtectionDomain$JavaSecurityAccessImpl.doIntersectionPrivilege(ProtectionDomain.java:87)
at java.desktop/java.awt.EventQueue.dispatchEvent(EventQueue.java:742)
at java.desktop/java.awt.EventDispatchThread.pumpOneEventForFilters(EventDispatchThread.java:203)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForFilter(EventDispatchThread.java:124)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForHierarchy(EventDispatchThread.java:113)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:109)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:101)
at java.desktop/java.awt.EventDispatchThread.run(EventDispatchThread.java:90)
}
FreeMarker template error (DEBUG mode; use RETHROW in production!):
The following has evaluated to null or missing:
==> RTOS.tasks [in template "freertos_body_user_threads_cmsis_v2.ftl" at line 7, column 12]
----
Tip: It's the step after the last dot that caused this error, not those before it.
----
Tip: If the failing expression is known to legally refer to something that's sometimes null or missing, either specify a default value like myOptionalVar!myDefault, or use [#if myOptionalVar??]when-present[#else]when-missing[/#if]. (These only cover the last step of the expression; to cover the whole expression, use parenthesis: (myOptionalVar.foo)!myDefault, (myOptionalVar.foo)??
----
----
FTL stack trace ("~" means nesting-related):
- Failed at: #list RTOS.tasks as task [in template "freertos_body_user_threads_cmsis_v2.ftl" at line 7, column 5]
----
Java stack trace (for programmers):
----
freemarker.core.InvalidReferenceException: [... Exception message was already printed; see it above ...]
at freemarker.core.InvalidReferenceException.getInstance(InvalidReferenceException.java:134)
at freemarker.core.Expression.assertNonNull(Expression.java:249)
at freemarker.core.IteratorBlock.acceptWithResult(IteratorBlock.java:104)
at freemarker.core.IteratorBlock.accept(IteratorBlock.java:94)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.visit(Environment.java:376)
at freemarker.core.Environment.visitAndTransform(Environment.java:501)
at freemarker.core.CompressedBlock.accept(CompressedBlock.java:42)
at freemarker.core.Environment.visit(Environment.java:334)
at freemarker.core.Environment.process(Environment.java:313)
at freemarker.template.Template.process(Template.java:383)
at com.st.microxplorer.codegenerator.CodeEngine.freemarkerDo(CodeEngine.java:420)
at com.st.microxplorer.codegenerator.CodeEngine.genCode(CodeEngine.java:273)
at com.st.microxplorer.codegenerator.CodeGenerator.generateOutputCode(CodeGenerator.java:6459)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCode(CodeGenerator.java:5281)
at com.st.microxplorer.codegenerator.CodeGenerator.generateSpecificCodeFile(CodeGenerator.java:1862)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCodeFiles(CodeGenerator.java:2188)
at com.st.microxplorer.codegenerator.CodeGenerator.generateCode(CodeGenerator.java:1691)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.generateCode(ProjectBuilder.java:3343)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createCode(ProjectBuilder.java:2235)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:819)
at com.st.microxplorer.plugins.projectmanager.engine.ProjectBuilder.createProject(ProjectBuilder.java:608)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager.startGenerateCode(MainProjectManager.java:1497)
at com.st.microxplorer.plugins.projectmanager.engine.MainProjectManager$3.onCommandEntered(MainProjectManager.java:283)
at com.st.components.util.CommandLineManager$3.run(CommandLineManager.java:175)
at java.desktop/java.awt.event.InvocationEvent.dispatch(InvocationEvent.java:308)
at java.desktop/java.awt.EventQueue.dispatchEventImpl(EventQueue.java:773)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:720)
at java.desktop/java.awt.EventQueue$4.run(EventQueue.java:714)
at java.base/java.security.AccessController.doPrivileged(AccessController.java:400)
at java.base/java.security.ProtectionDomain$JavaSecurityAccessImpl.doIntersectionPrivilege(ProtectionDomain.java:87)
at java.desktop/java.awt.EventQueue.dispatchEvent(EventQueue.java:742)
at java.desktop/java.awt.EventDispatchThread.pumpOneEventForFilters(EventDispatchThread.java:203)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForFilter(EventDispatchThread.java:124)
at java.desktop/java.awt.EventDispatchThread.pumpEventsForHierarchy(EventDispatchThread.java:113)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:109)
at java.desktop/java.awt.EventDispatchThread.pumpEvents(EventDispatchThread.java:101)
at java.desktop/java.awt.EventDispatchThread.run(EventDispatchThread.java:90)

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Log horodaté. osKernelGetTickCount() = millisecondes depuis le démarrage
 * de FreeRTOS (équivalent RTOS de HAL_GetTick). */
static void log_etat(const char *etat)
{
  printf("[%8lu ms] <%s>\r\n", (unsigned long)osKernelGetTickCount(), etat);
}

/**
  * @brief  Tâche de lecture des capteurs.
  *         LED rouge = occupé (lecture en cours).
  */
void TacheCapteurs(void *argument)
{
  (void)argument;   /* paramètre non utilisé : évite un warning */

  /* --- Initialisation : exécutée UNE fois, au démarrage de la tâche --- */
  led_on(LED_ID_RED);
  log_etat("scan du bus I2C");
  sensors_i2c_scan(&hi2c1);

  log_etat("initialisation des capteurs");
  int nb = sensors_init(&hi2c1);
  printf("  %d/3 capteurs prets\r\n", nb);
  led_off(LED_ID_RED);

  /* --- Boucle infinie : une tâche ne doit JAMAIS se terminer --- */
  for (;;)
  {
    led_on(LED_ID_RED);                 /* occupé */
    log_etat("Lecture Interface Capteurs");
    sensors_read(&s_data);
    sensors_print(&s_data);

    /* Avant, on faisait une attente active (while ... HAL_GetTick) pour que
     * la LED rouge reste visible. Avec FreeRTOS on DORT : pendant ces 300 ms,
     * TacheLed peut continuer à faire clignoter la LED verte. */
    osDelay(LED_BUSY_MIN_MS);
    led_off(LED_ID_RED);                /* lecture terminée */

    /* La tâche dort jusqu'à la prochaine lecture : le CPU est libre. */
    osDelay(SENSOR_PERIOD_MS - LED_BUSY_MIN_MS);
  }
}

/**
  * @brief  Tâche "signe de vie" : la LED verte clignote tant que
  *         l'ordonnanceur tourne. Si elle s'arrête, le système est bloqué.
  */
void TacheLed(void *argument)
{
  (void)argument;

  for (;;)
  {
    led_toggle(LED_ID_GREEN);
    osDelay(LED_BLINK_MS);
  }
}

/* USER CODE END Application */

