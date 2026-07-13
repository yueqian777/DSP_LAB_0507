$ErrorActionPreference = 'Stop'

$project = 'C:\Users\zhangyueqian\lab8\DSP_LAB_0507\projects\2302_10313v1_cn_group_report'
$pptx = Get-ChildItem -LiteralPath (Join-Path $project 'exports') -Filter '*.pptx' |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if ($null -eq $pptx) {
    throw 'No PPTX found in exports.'
}

$figDir = Join-Path $project 'sources\2302.10313v1_files'
$assetDir = Join-Path $project 'analysis\assets'
$nl = [Environment]::NewLine

function Get-Rgb {
    param([int]$R, [int]$G, [int]$B)
    return $R + 256 * $G + 65536 * $B
}

$blue = Get-Rgb 46 105 178
$darkBlue = Get-Rgb 28 73 123
$lightBlue = Get-Rgb 224 236 249
$paleBlue = Get-Rgb 241 247 253
$midGray = Get-Rgb 96 108 120
$lightGray = Get-Rgb 238 242 246
$white = Get-Rgb 255 255 255

$msoFalse = 0
$msoTrue = -1
$msoTextOrientationHorizontal = 1
$msoShapeRoundedRectangle = 5
$msoShapeOval = 9
$msoConnectorStraight = 1
$msoAlignCenter = 2
$msoAnchorMiddle = 3

$script:EquationCount = 0

function Get-BodyShape {
    param($Slide)
    $candidates = @()
    foreach ($shape in $Slide.Shapes) {
        if ($shape.HasTextFrame -eq $msoTrue -and
            $shape.TextFrame.HasText -eq $msoTrue -and
            $shape.Left -gt 440 -and
            $shape.Top -gt 70 -and
            $shape.Top -lt 300 -and
            $shape.Width -gt 350) {
            $candidates += $shape
        }
    }
    if ($candidates.Count -eq 0) {
        throw "Body shape not found on slide $($Slide.SlideIndex)."
    }
    return $candidates | Sort-Object Top | Select-Object -First 1
}

function Set-BodyLayout {
    param(
        $Slide,
        [single]$Height,
        [single]$FontSize = 0
    )
    $body = Get-BodyShape $Slide
    $body.TextFrame2.AutoSize = 0
    $body.Height = $Height
    if ($FontSize -gt 0) {
        $body.TextFrame2.TextRange.Font.Size = $FontSize
    }
    return $body
}

function Clear-LeftVisuals {
    param($Slide)
    for ($i = $Slide.Shapes.Count; $i -ge 1; $i--) {
        $shape = $Slide.Shapes.Item($i)
        if ($shape.Type -eq 13 -and $shape.Left -lt 430 -and $shape.Top -gt 35) {
            $shape.Delete()
        }
    }
}

function Add-PictureFit {
    param(
        $Slide,
        [string]$Path,
        [single]$Left,
        [single]$Top,
        [single]$Width,
        [single]$Height
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Image not found: $Path"
    }
    $shape = $Slide.Shapes.AddPicture($Path, $msoFalse, $msoTrue, 0, 0, -1, -1)
    $scale = [Math]::Min($Width / $shape.Width, $Height / $shape.Height)
    $newWidth = $shape.Width * $scale
    $newHeight = $shape.Height * $scale
    $shape.LockAspectRatio = $msoFalse
    $shape.Width = $newWidth
    $shape.Height = $newHeight
    $shape.Left = $Left + ($Width - $newWidth) / 2
    $shape.Top = $Top + ($Height - $newHeight) / 2
    $shape.LockAspectRatio = $msoTrue
    return $shape
}

function Add-PlainText {
    param(
        $Slide,
        [string]$Text,
        [single]$Left,
        [single]$Top,
        [single]$Width,
        [single]$Height,
        [single]$FontSize = 15,
        [int]$Color = 0,
        [int]$Alignment = 2,
        [string]$FontName = 'Microsoft YaHei'
    )
    $shape = $Slide.Shapes.AddTextbox($msoTextOrientationHorizontal, $Left, $Top, $Width, $Height)
    $shape.TextFrame2.AutoSize = 0
    $shape.TextFrame2.VerticalAnchor = $msoAnchorMiddle
    $shape.TextFrame2.MarginLeft = 4
    $shape.TextFrame2.MarginRight = 4
    $shape.TextFrame2.MarginTop = 2
    $shape.TextFrame2.MarginBottom = 2
    $shape.TextFrame2.TextRange.Text = $Text
    $shape.TextFrame2.TextRange.ParagraphFormat.Alignment = $Alignment
    $shape.TextFrame2.TextRange.Font.Name = $FontName
    $shape.TextFrame2.TextRange.Font.Size = $FontSize
    $shape.TextFrame2.TextRange.Font.Fill.ForeColor.RGB = $Color
    return $shape
}

function Add-Node {
    param(
        $Slide,
        [string]$Text,
        [single]$Left,
        [single]$Top,
        [single]$Width,
        [single]$Height,
        [int]$FillColor = 0,
        [int]$LineColor = 0,
        [int]$TextColor = 0,
        [single]$FontSize = 15,
        [int]$ShapeType = 5
    )
    $shape = $Slide.Shapes.AddShape($ShapeType, $Left, $Top, $Width, $Height)
    $shape.Fill.ForeColor.RGB = $FillColor
    $shape.Fill.Transparency = 0
    $shape.Line.ForeColor.RGB = $LineColor
    $shape.Line.Weight = 1.4
    $shape.TextFrame2.AutoSize = 0
    $shape.TextFrame2.VerticalAnchor = $msoAnchorMiddle
    $shape.TextFrame2.MarginLeft = 6
    $shape.TextFrame2.MarginRight = 6
    $shape.TextFrame2.MarginTop = 3
    $shape.TextFrame2.MarginBottom = 3
    $shape.TextFrame2.TextRange.Text = $Text
    $shape.TextFrame2.TextRange.ParagraphFormat.Alignment = $msoAlignCenter
    $shape.TextFrame2.TextRange.Font.Name = 'Microsoft YaHei'
    $shape.TextFrame2.TextRange.Font.Size = $FontSize
    $shape.TextFrame2.TextRange.Font.Fill.ForeColor.RGB = $TextColor
    return $shape
}

function Add-Arrow {
    param(
        $Slide,
        [single]$X1,
        [single]$Y1,
        [single]$X2,
        [single]$Y2
    )
    $line = $Slide.Shapes.AddConnector($msoConnectorStraight, $X1, $Y1, $X2, $Y2)
    $line.Line.ForeColor.RGB = $blue
    $line.Line.Weight = 1.6
    $line.Line.EndArrowheadStyle = 3
    return $line
}

function Copy-BuiltOMath {
    param($WordDoc, [string]$Linear)
    $content = $WordDoc.Content
    [void]$content.Delete()
    $range = $WordDoc.Range(0, 0)
    $range.Text = $Linear
    [void]$range.OMaths.Add($range)
    $omath = $WordDoc.OMaths.Item(1)
    $omath.BuildUp()
    $omath.Range.Copy()
}

function Add-NativeEquation {
    param(
        $Slide,
        $WordDoc,
        [string]$Linear,
        [single]$Left,
        [single]$Top,
        [single]$Width,
        [single]$Height,
        [single]$FontSize = 19,
        [int]$Color = 0
    )
    Copy-BuiltOMath $WordDoc $Linear
    $shape = $Slide.Shapes.AddTextbox($msoTextOrientationHorizontal, $Left, $Top, $Width, $Height)
    $shape.TextFrame2.AutoSize = 0
    $shape.TextFrame2.VerticalAnchor = $msoAnchorMiddle
    $shape.TextFrame2.MarginLeft = 0
    $shape.TextFrame2.MarginRight = 0
    $shape.TextFrame2.MarginTop = 0
    $shape.TextFrame2.MarginBottom = 0
    $shape.TextFrame2.TextRange.Text = ''
    $pasted = $shape.TextFrame2.TextRange.Paste()
    $pasted.Font.Name = 'Cambria Math'
    $pasted.Font.Size = $FontSize
    $pasted.Font.Fill.ForeColor.RGB = $Color
    $shape.TextFrame2.TextRange.ParagraphFormat.Alignment = $msoAlignCenter
    $zones = $shape.TextFrame2.TextRange.MathZones.Invoke(1, -1)
    if ($zones.Count -lt 1) {
        throw "Native equation verification failed on slide $($Slide.SlideIndex): $Linear"
    }
    $script:EquationCount++
    return $shape
}

$ppt = $null
$pres = $null
$word = $null
$wordDoc = $null
try {
    $ppt = New-Object -ComObject PowerPoint.Application
    $pres = $ppt.Presentations.Open($pptx.FullName, $msoFalse, $msoFalse, $msoFalse)
    if ($pres.Slides.Count -ne 25) {
        throw "Unexpected slide count: $($pres.Slides.Count)"
    }

    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $wordDoc = $word.Documents.Add()

    $s = $pres.Slides.Item(3)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'paper_title.png') 35 62 345 120 | Out-Null
    Add-Node $s '① 低信噪比下不依赖 VAD' 52 215 310 62 $paleBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '② 面向 DSK6713 实时处理' 52 300 310 62 $lightBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '③ 尽量保留语音可懂度' 52 385 310 62 $paleBlue $blue $darkBlue 15 | Out-Null

    $s = $pres.Slides.Item(4)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p1_0.png') 35 105 350 330 | Out-Null
    Set-BodyLayout $s 155 | Out-Null
    Add-NativeEquation $s $wordDoc 'x(t)=s(t)+n(t) ⇔ X(ω)=S(ω)+N(ω)' 480 260 390 48 18 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc '|Y(ω)|=max(0,|X(ω)|-|N̂(ω)|)' 480 310 390 52 19 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'Y(ω)=g(ω)X(ω), g(ω)=max(0,1-(|N̂(ω)|)/(|X(ω)|+ε))' 475 362 400 72 16 $darkBlue | Out-Null

    $s = $pres.Slides.Item(5)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p2_1.png') 38 58 340 205 | Out-Null
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p2_2.png') 38 275 340 230 | Out-Null
    Set-BodyLayout $s 215 | Out-Null
    Add-NativeEquation $s $wordDoc 'w[n]=√((1-0.85185 cos((2n+1)π/N))/4), 0≤n<N' 475 345 400 76 16 $darkBlue | Out-Null

    $s = $pres.Slides.Item(7)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'mmse_fig5.png') 35 72 345 215 | Out-Null
    $minimumText = '10 s 历史窗口' + $nl + '≈ 1250 帧' + $nl + '逐频点最小值 → α 补偿'
    Add-Node $s $minimumText 65 320 285 135 $paleBlue $blue $darkBlue 17 | Out-Null

    $s = $pres.Slides.Item(8)
    Clear-LeftVisuals $s
    Add-Arrow $s 207 135 207 175 | Out-Null
    Add-Arrow $s 207 220 207 260 | Out-Null
    Add-Arrow $s 207 305 207 345 | Out-Null
    Add-Arrow $s 207 390 207 430 | Out-Null
    Add-Node $s 'M₁：当前 2.5 s' 72 85 270 50 $lightBlue $blue $darkBlue 16 | Out-Null
    Add-Node $s 'M₂：上一桶' 72 175 270 45 $paleBlue $blue $darkBlue 16 | Out-Null
    Add-Node $s 'M₃：更早一桶' 72 260 270 45 $paleBlue $blue $darkBlue 16 | Out-Null
    Add-Node $s 'M₄：最旧一桶' 72 345 270 45 $paleBlue $blue $darkBlue 16 | Out-Null
    Add-Node $s '逐频点取 min(M₁,M₂,M₃,M₄)' 55 430 305 55 $blue $blue $white 15 | Out-Null
    Set-BodyLayout $s 188 | Out-Null
    Add-NativeEquation $s $wordDoc 'M_1(ω)=min(|X(ω)|,M_1(ω))' 480 302 390 50 19 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'N̂(ω)=min(M_1(ω),M_2(ω),M_3(ω),M_4(ω))' 475 360 400 58 17 $darkBlue | Out-Null

    $s = $pres.Slides.Item(9)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'alpha20_fig6.png') 42 70 335 185 | Out-Null
    Add-PlainText $s 'α=20：宽带残留较多' 55 245 310 24 12 $midGray | Out-Null
    Add-PictureFit $s (Join-Path $assetDir 'alpha200_fig7.png') 42 285 335 185 | Out-Null
    Add-PlainText $s 'α=200：孤立谱峰更突出' 55 470 310 24 12 $midGray | Out-Null

    $s = $pres.Slides.Item(10)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'spectral_floor_fig8.png') 35 75 350 400 | Out-Null
    Set-BodyLayout $s 210 | Out-Null
    Add-NativeEquation $s $wordDoc 'g(ω)=max(λ,1-α(|N̂(ω)|)/(|X(ω)|+ε))' 475 348 400 72 19 $darkBlue | Out-Null

    $s = $pres.Slides.Item(12)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'fft_and_mag_code.png') 35 55 350 210 | Out-Null
    Add-PictureFit $s (Join-Path $assetDir 'mmse_code.png') 35 275 350 230 | Out-Null
    Set-BodyLayout $s 210 | Out-Null
    Add-NativeEquation $s $wordDoc 'X[N-k]=X^*[k], k=1,…,N/2-1' 480 350 390 62 20 $darkBlue | Out-Null

    $s = $pres.Slides.Item(13)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p5_4.png') 42 65 335 190 | Out-Null
    Add-PlainText $s 'car1 输入' 55 245 310 22 12 $midGray | Out-Null
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p5_5.png') 42 285 335 190 | Out-Null
    Add-PlainText $s '基础谱减输出（α=20，λ=0.05）' 48 472 325 24 11 $midGray | Out-Null

    $s = $pres.Slides.Item(14)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p6_6.png') 35 75 350 315 | Out-Null
    Add-Node $s '作者组合改动：α 20 → 2，τ = 30 ms' 55 415 310 62 $paleBlue $blue $darkBlue 15 | Out-Null
    Set-BodyLayout $s 215 | Out-Null
    Add-NativeEquation $s $wordDoc 'P_t(ω)=(1-k)|X_t(ω)|+kP_(t-1)(ω), k=e^(-T/τ)' 472 345 405 78 16 $darkBlue | Out-Null

    $s = $pres.Slides.Item(15)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p6_7.png') 35 65 350 315 | Out-Null
    Add-Node $s '幅度' 42 410 96 55 $paleBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '功率' 155 410 96 55 $lightBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '噪声估计' 268 410 105 55 $paleBlue $blue $darkBlue 15 | Out-Null
    Set-BodyLayout $s 205 | Out-Null
    Add-NativeEquation $s $wordDoc 'g(ω)=max(λ(|N̂(ω)|)/(|P(ω)|+ε),1-α(|N̂(ω)|)/(|P(ω)|+ε))' 470 338 410 88 15 $darkBlue | Out-Null

    $s = $pres.Slides.Item(16)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p7_8.png') 40 65 340 205 | Out-Null
    Add-PictureFit $s (Join-Path $assetDir 'snr_bins_fig23.png') 40 290 340 205 | Out-Null
    Set-BodyLayout $s 105 18 | Out-Null
    Add-PlainText $s '按图 22 实线重构（原文式 16 有矛盾）' 475 280 400 22 11 $midGray | Out-Null
    Add-NativeEquation $s $wordDoc 'α=5, SNR<-5' 475 305 400 28 16 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'α=4.2-0.16 SNR, -5≤SNR≤20' 475 335 400 30 16 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'α=1, SNR>20' 475 368 400 28 16 $darkBlue | Out-Null

    $s = $pres.Slides.Item(17)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p8_10.png') 42 65 335 190 | Out-Null
    Add-PlainText $s '直升机噪声输入' 55 246 310 22 12 $midGray | Out-Null
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p8_11.png') 42 285 335 190 | Out-Null
    Add-PlainText $s '加入 δ(F) 后的输出' 55 472 310 22 12 $midGray | Out-Null
    Set-BodyLayout $s 100 18 | Out-Null
    Add-NativeEquation $s $wordDoc 'g(ω)=max(λ,1-δ(F)α(SNR)(|N̂(ω)|)/(|X(ω)|+ε))' 470 282 410 60 15 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'δ=1, 0<F<1 kHz' 475 344 400 24 14 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'δ=2.5, 1≤F<2 kHz' 475 370 400 24 14 $darkBlue | Out-Null
    Add-NativeEquation $s $wordDoc 'δ=1.5, F≥2 kHz' 475 396 400 24 14 $darkBlue | Out-Null

    $s = $pres.Slides.Item(18)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'window_response_fig4.png') 42 65 335 190 | Out-Null
    Add-PlainText $s '窗函数频率响应' 55 246 310 22 12 $midGray | Out-Null
    Add-PictureFit $s (Join-Path $figDir '2302.10313v1_p7_9.png') 42 285 335 190 | Out-Null
    Add-PlainText $s '有色车噪频谱' 55 472 310 22 12 $midGray | Out-Null

    $s = $pres.Slides.Item(19)
    Clear-LeftVisuals $s
    $ys = @(70, 138, 206, 274, 342, 410)
    for ($i = 0; $i -lt $ys.Count - 1; $i++) {
        Add-Arrow $s 208 ($ys[$i] + 46) 208 $ys[$i + 1] | Out-Null
    }
    $labels = @('分帧 + 归一化窗', '完整 FFT', '半谱 MMSE / 平滑', 'α(SNR) × δ(F)', '谱下限 λ', 'IFFT + 重叠相加')
    for ($i = 0; $i -lt $labels.Count; $i++) {
        $fill = if ($i % 2 -eq 0) { $paleBlue } else { $lightBlue }
        Add-Node $s $labels[$i] 65 $ys[$i] 286 46 $fill $blue $darkBlue 14 | Out-Null
    }

    $s = $pres.Slides.Item(21)
    Clear-LeftVisuals $s
    Add-Node $s ('主观听感' + $nl + '无听众数 / 无量表') 55 85 310 95 $paleBlue $blue $darkBlue 17 | Out-Null
    Add-Node $s ('时频谱图' + $nl + '可见残留，但不等于可懂度') 55 220 310 95 $lightBlue $blue $darkBlue 17 | Out-Null
    Add-Node $s ('输入 / 输出 SNR' + $nl + '定义与统计方法未交代') 55 355 310 95 $paleBlue $blue $darkBlue 17 | Out-Null

    $s = $pres.Slides.Item(22)
    Clear-LeftVisuals $s
    Add-PictureFit $s (Join-Path $assetDir 'final_snr_fig27.png') 35 95 350 330 | Out-Null
    Add-Node $s 'phantom4：输入→输出 SNR 增益 5.98 dB' 48 435 325 58 $paleBlue $blue $darkBlue 14 | Out-Null

    $s = $pres.Slides.Item(23)
    Clear-LeftVisuals $s
    Add-Node $s ('贡献' + $nl + '无需 VAD；面向实时硬件；系统比较多种增强') 48 80 325 165 $lightBlue $blue $darkBlue 17 | Out-Null
    Add-Node $s ('局限' + $nl + '8 kHz 窄带；跟踪偏慢；缺少客观指标与实时资源数据') 48 285 325 185 $lightGray $midGray $darkBlue 17 | Out-Null

    $s = $pres.Slides.Item(24)
    Clear-LeftVisuals $s
    Add-Arrow $s 145 155 190 235 | Out-Null
    Add-Arrow $s 305 155 235 235 | Out-Null
    Add-Arrow $s 145 385 190 305 | Out-Null
    Add-Arrow $s 305 385 235 305 | Out-Null
    Add-Node $s '最小幅度谱估计' 48 92 185 70 $paleBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '跨帧平滑' 245 92 125 70 $lightBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s 'α / δ 抑制强度' 48 378 185 70 $lightBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s 'λ 谱下限' 245 378 125 70 $paleBlue $blue $darkBlue 15 | Out-Null
    Add-Node $s '输出质量' 155 220 110 100 $blue $blue $white 18 $msoShapeOval | Out-Null
    Set-BodyLayout $s 200 | Out-Null
    Add-NativeEquation $s $wordDoc 'g(ω)=max(λ,1-δ(F)α(SNR)(|N̂(ω)|)/(|X(ω)|+ε))' 470 342 410 82 16 $darkBlue | Out-Null

    $pres.Save()
    Write-Output "Post-processed PPTX: $($pptx.FullName)"
    Write-Output "Native equations inserted: $script:EquationCount"
}
finally {
    if ($wordDoc -ne $null) {
        $wordDoc.Close($false)
    }
    if ($word -ne $null) {
        $word.Quit()
    }
    if ($pres -ne $null) {
        $pres.Close()
    }
    if ($ppt -ne $null) {
        $ppt.Quit()
    }
    [System.GC]::Collect()
    [System.GC]::WaitForPendingFinalizers()
}
