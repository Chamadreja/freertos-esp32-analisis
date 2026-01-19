# Análisis de FreeRTOS para ESP32: Modelización, Planificación y Memoria

Este repositorio contiene un estudio técnico profundo sobre la implementación y el comportamiento de **FreeRTOS** en microcontroladores **ESP32**, utilizando el framework **ESP-IDF**. El trabajo fue realizado para la materia **Sistemas Operativos y Redes** en la UNRN.

## Áreas de Análisis Técnico

### 1. Modelización y Estructura de Procesos
- **Task Control Block (TCB):** Análisis de la estructura de datos que permite a FreeRTOS gestionar el estado, la prioridad y el contexto de cada tarea.
- **Estados de Tareas:** Estudio del ciclo de vida de una tarea (Running, Ready, Blocked, Suspended) y los eventos que disparan las transiciones.

### 2. Planificación en Arquitecturas Multinúcleo (SMP)
- **Symmetric Multiprocessing (SMP):** Investigación de cómo la versión de FreeRTOS de ESP-IDF adapta el kernel para gestionar los dos núcleos del ESP32, permitiendo la ejecución simultánea de tareas.
- **Algoritmo de Planificación:** Análisis del esquema de planificación por prioridad fija con desalojo (preemptive) y asignación de cuotas de tiempo (Time Slicing).

### 3. Gestión de Memoria y Algoritmo TLSF
- **Estrategias de Asignación:** Estudio de la gestión del Heap en sistemas de tiempo real y el uso de `pvPortMalloc()`.
- **Algoritmo TLSF (Two-Level Segregated Fit):** Análisis detallado de este algoritmo utilizado en ESP-IDF para lograr una asignación de memoria dinámica con tiempo de ejecución constante $O(1)$ y fragmentación mínima.

## Tecnologías y Herramientas
- **Kernel:** FreeRTOS (Versión optimizada para ESP32).
- **Framework:** ESP-IDF (Espressif IoT Development Framework).
- **Hardware de Referencia:** Microcontrolador ESP32 (Arquitectura Xtensa® Dual-Core).

## Contenido del Repositorio
- `/documentacion`: Informe técnico detallado sobre la arquitectura del RTOS.
- `/programas`: Un programa sobre la implementación de Memoria Virtual.
