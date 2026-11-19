# analyse_npu.ps1 - Analyse des modèles int8 pour le NPU Neural-ART du STM32N6
#
# Lance "stedgeai analyze" avec l'option --st-neural-art sur les modèles
# quantifiés (modele_meteo_h0_int8.tflite, modele_meteo_h3_int8.tflite).
# Le rapport indique, couche par couche, ce qui serait exécuté par le NPU et
# ce qui resterait sur le Cortex-M55, ainsi que la mémoire nécessaire.
# Le projet firmware n'est pas modifié.
#
# Utilisation (PowerShell, depuis la racine du dépôt) :
#   powershell -ExecutionPolicy Bypass -File ml\analyse_npu.ps1
#
# Résultats : st_ai_output\npu_h0\ et st_ai_output\npu_h3\ (rapport),
#             st_ai_output\npu_h0.log et st_ai_output\npu_h3.log (console).
# Le dossier st_ai_output\ est exclu du dépôt (.gitignore).

$ErrorActionPreference = "Continue"   # stedgeai écrit sur stderr : ne pas arrêter le script

# stedgeai est fourni par le pack X-CUBE-AI installé par STM32CubeMX
$pack   = "$env:USERPROFILE\STM32Cube\Repository\Packs\STMicroelectronics\X-CUBE-AI\10.2.0"
$st     = "$pack\Utilities\windows\stedgeai.exe"
$profil = "$pack\scripts\N6_scripts\user_neuralart.json"

if (-not (Test-Path $st)) {
    Write-Host "stedgeai introuvable : $st"
    Write-Host "Installer X-CUBE-AI 10.2.0 (STM32CubeMX > Manage software packs)."
    exit 1
}

New-Item -ItemType Directory -Force -Path "st_ai_output" | Out-Null

foreach ($h in @("h0", "h3")) {
    $modele = "ml\models\export\modele_meteo_${h}_int8.tflite"
    $sortie = "st_ai_output\npu_$h"
    $log    = "st_ai_output\npu_$h.log"
    if (-not (Test-Path $modele)) {
        Write-Host "Modèle absent : $modele (lancer ml\04_exporter.py)"
        continue
    }

    Write-Host "=== $h : analyse pour le NPU"
    # Profil par défaut ; si l'outil le refuse, profil fourni avec le pack
    & $st analyze -m $modele --target stm32n6 --st-neural-art `
        --name "meteo_${h}_npu" -o $sortie 2>&1 | Tee-Object -FilePath $log
    if ($LASTEXITCODE -ne 0 -and (Test-Path $profil)) {
        Write-Host "--- nouvel essai avec le profil $profil"
        & $st analyze -m $modele --target stm32n6 --st-neural-art "default@$profil" `
            --name "meteo_${h}_npu" -o $sortie 2>&1 | Tee-Object -FilePath $log
    }
}

Write-Host ""
Write-Host "Rapports : st_ai_output\npu_h0\ et st_ai_output\npu_h3\"
