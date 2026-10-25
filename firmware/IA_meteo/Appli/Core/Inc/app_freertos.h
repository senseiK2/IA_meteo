/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : app_freertos.h
  * Description        : FreeRTOS applicative header file
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


/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __APP_FREERTOS_H
#define __APP_FREERTOS_H

#ifdef __cplusplus
extern "C" {
#endif
/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h" 
#include "cmsis_os2.h"
#n
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Exported macro -------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */
FreeMarker template error (DEBUG mode; use RETHROW in production!):
The following has evaluated to null or missing:
==> RTOS.tasks [in template "freertos_app_h.ftl" at line 93, column 12]
----
Tip: It's the step after the last dot that caused this error, not those before it.
----
Tip: If the failing expression is known to legally refer to something that's sometimes null or missing, either specify a default value like myOptionalVar!myDefault, or use [#if myOptionalVar??]when-present[#else]when-missing[/#if]. (These only cover the last step of the expression; to cover the whole expression, use parenthesis: (myOptionalVar.foo)!myDefault, (myOptionalVar.foo)??
----
----
FTL stack trace ("~" means nesting-related):
- Failed at: #list RTOS.tasks as task [in template "freertos_app_h.ftl" at line 93, column 5]
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
at freemarker.core.Environment.visit(Environment.java:340)
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
