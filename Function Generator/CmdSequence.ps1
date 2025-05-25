# Press 'Ctrl-C' to exit.
# Command line parameters...
Param (
    [string]$FileName   = ".\Demo1.txt",
    [string]$SerialPort = "COM12"
)

$host.UI.RawUI.BackgroundColor = 'Black'        # Need to close and re-open session to take effect
$error.clear()                                  # clear previous errors

$tmp = [System.IO.Ports.SerialPort]::getportnames()
Write-Host  -Ba Black -Fo DarkGray "Available serial ports:" $tmp

$Port = new-Object System.IO.Ports.SerialPort

function InitPort {
    $Port.PortName                  = $SerialPort
    $Port.BaudRate                  = 115200
    $Port.Parity                    = 'None'
    $Port.DataBits                  = 8
    $Port.StopBits                  = 1
    $Port.Handshake                 = 'XOnXOff'
    $Port.DTREnable                 = $True
    $port.NewLine                   = ">"        # Cursor = End of transmition
    $port.ReadTimeout               = 2000      # 5 sec
    $port.WriteTimeout              = 2000      #  5 sec

    Write-Host -Ba Black -Fo DarkGray "Opening Serial Port   :" $Port.portname
    $Port.Open()
}

function SndCmd($cmd) {
    try {
        $Port.Write($cmd)
    }
    catch [TimeoutException] {
        Write-Host -Fo Red "`nERROR:  TimeoutException in WriteLine"
        Write-Error "ERROR:  ${Error}"
        break
    }
    try {
        $line = $Port.ReadLine()
        $line = $line -replace ".{1}$"                     # Drop the final LF as it messes up screen formating
        $line += ">"                                       # Serial newline character gets stripped, so put it back
        $Command = $line.Substring(0,10)                   # Command = first 10 characters
        $Response = $line.Substring(10,$line.length-11)    # Response = Remaining characters
        $Cursor = $line.Substring($line.Length-1)          # Cursor (actually printed on next line)
           }
    catch [TimeoutException] {
        Write-Host -Fo Red "`nERROR:  TimeoutException in ReadLine"
        Write-Error "ERROR:  ${Error}"
        break
    }
#   printable($line)
    Write-Host -NoNewline -Ba Black -Fo Green $Command
    Write-Host -NoNewline -Ba Black -Fo Cyan $Response
    Write-Host -NoNewline -Ba Black -Fo White $Cursor
}

Function Get-FileName($initialDirectory) {  
     [System.Reflection.Assembly]::LoadWithPartialName(“System.windows.forms”) |
     Out-Null

     $OpenFileDialog = New-Object System.Windows.Forms.OpenFileDialog
     $openFileDialog.Title = "Select data file"
     $OpenFileDialog.initialDirectory = $initialDirectory
     $OpenFileDialog.filter = “text files (*.txt)| *.txt”
     $OpenFileDialog.ShowDialog() | Out-Null
     return $OpenFileDialog.filename
} #end function Get-FileName

function Colours() {
# DEBUG ROUTINE - shows all PowerShell colours.
    $colors = [enum]::GetValues([System.ConsoleColor])
    Foreach ($bgcolor in $colors){
        Foreach ($fgcolor in $colors) { Write-Host "$fgcolor|"  -ForegroundColor $fgcolor -BackgroundColor $bgcolor -NoNewLine }
        Write-Host " on $bgcolor"
    }
} #end function Colours

function Printable([string] $s) {
# DEBUG ROUTINE - shows white space characters contained in a string.
# Function to display CRLF (whitespace) characters.
     $Matcher = { param($m) 
                  $x = $m.Groups[0].Value
                  $c = [int]($x.ToCharArray())[0]
                  switch ($c)
                  {   9 { '\t' }
                      13 { '\r' }
                      10 { '\n' }
                      92 { '\\' }
                      Default { "\$c" } }
                 }
     return ([regex]'[^ -~\\]').Replace($s, $Matcher)
} #end function Printable

InitPort
#$FileName = Get-FileName(".\")

Write-Host -Fo DarkGray "Opening file          : " $FileName

$FileData = Get-Content -Path $FileName
try {
    Write-Host -NoNewline -Ba Black -Fo White ">"
# Loop until Ctrl-C press...
    while ($true) { 
        foreach ($line in $FileData) {
            # Write-Host -Ba Black -Fo Yellow $line
            $Parms = $line.Split(" ")
            If ($Parms[0].ToLower() -eq "pause") {
                $Period = [int]$Parms[1]                           # Convert to integer
                $tmp = $line.Substring($line.Length-2).ToLower()   # Last two characters of line
                Write-Host -NoNewline -Ba Black -Fo White $Cursor
                if ($tmp -eq "ms") { 
                    Write-Host -Ba Black -Fo Green "           Pause" $Period "milli-seconds"
                    Start-sleep -Milliseconds $Period
                } else { 
                    Write-Host -Ba Black -Fo Green "           Pause" $Period "seconds"
                    Start-sleep $Period 
                }
                Write-Host -NoNewline -Ba Black -Fo White ">"
            }
            elseif ($Parms[0] -eq "#") {
                Write-Host -Ba Black -Fo Green $line.Substring(2)
                Write-Host -NoNewline -Ba Black -Fo White ">"
            }
            else { SndCmd($line + "`n") }
        }
    }
}
finally {
# Ctrl-C falls through here...
    Write-Host -Ba Black -Fo DarkGray "Closing Serial Port" $Port.portname
    $Port.Close()
}

