$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$project = 'C:\Users\zhangyueqian\lab8\DSP_LAB_0507\projects\2302_10313v1_cn_group_report'
$pages = Join-Path $project 'analysis\paper_pages'
$outDir = Join-Path $project 'analysis\assets'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

function Save-Crop {
    param(
        [string]$Source,
        [string]$Name,
        [int]$X,
        [int]$Y,
        [int]$Width,
        [int]$Height
    )

    $srcPath = Join-Path $pages $Source
    $dstPath = Join-Path $outDir $Name
    $image = [System.Drawing.Bitmap]::FromFile($srcPath)
    try {
        $bounds = New-Object System.Drawing.Rectangle(0, 0, $image.Width, $image.Height)
        $requested = New-Object System.Drawing.Rectangle($X, $Y, $Width, $Height)
        $crop = [System.Drawing.Rectangle]::Intersect($bounds, $requested)
        if ($crop.Width -le 0 -or $crop.Height -le 0) {
            throw "Invalid crop $Name"
        }
        $bitmap = New-Object System.Drawing.Bitmap($crop.Width, $crop.Height)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::White)
                $graphics.DrawImage($image, 0, 0, $crop, [System.Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $graphics.Dispose()
            }
            $bitmap.Save($dstPath, [System.Drawing.Imaging.ImageFormat]::Png)
        }
        finally {
            $bitmap.Dispose()
        }
    }
    finally {
        $image.Dispose()
    }
}

Save-Crop 'page-01.png' 'paper_title.png' 40 0 1450 300
Save-Crop 'page-02.png' 'window_response_fig4.png' 800 760 680 650
Save-Crop 'page-03.png' 'mmse_fig5.png' 760 120 610 270
Save-Crop 'page-03.png' 'alpha20_fig6.png' 760 620 610 270
Save-Crop 'page-03.png' 'alpha200_fig7.png' 760 1320 610 270
Save-Crop 'page-04.png' 'spectral_floor_fig8.png' 0 0 760 610
Save-Crop 'page-04.png' 'fft_and_mag_code.png' 760 0 760 700
Save-Crop 'page-04.png' 'mmse_code.png' 760 610 760 930
Save-Crop 'page-07.png' 'snr_bins_fig23.png' 70 1190 620 300
Save-Crop 'page-08.png' 'final_snr_fig27.png' 780 1480 620 320

Get-ChildItem -LiteralPath $outDir -Filter '*.png' | Select-Object Name, Length
