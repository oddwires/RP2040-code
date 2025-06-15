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
$Margin = 10

function InitPort {
    $Port.PortName                  = $SerialPort
    $Port.BaudRate                  = 115200
    $Port.Parity                    = 'None'
    $Port.DataBits                  = 8
    $Port.StopBits                  = 1
    $Port.Handshake                 = 'XOnXOff'
    $Port.DTREnable                 = $True
#    $port.NewLine                   = ">"       # Cursor = End of transmition
    $port.ReadTimeout               = 6000      # 5 sec
    $port.WriteTimeout              = 2000      #  5 sec

    Write-Host -Ba Black -Fo DarkGray "Opening Serial Port   :" $Port.portname
    $Port.Open()
}

function SndCmd($cmd) {
     $Port.Write($cmd)
     #Write-Host -NoNewline -Ba Black -Fo Yellow $cmd
     Start-Sleep -Milliseconds 100
     while ($line = $Port.ReadLine()) {
         if ($line.startswith(">")) {                       # All commands have a '>' prompt
            $Command = $line
            Write-Host -NoNewline -Ba Black -Fo Green $Command`n
         }
         else {
            $command = $(" " * $Margin)
            $Response = $line.Substring(10,$line.length-11)    # Response = Remaining characters
            $Cursor = $line.Substring($line.Length-1)          # Cursor (actually printed on next line)
            Write-Host -NoNewline -Ba Black -Fo Green $Command

            Write-Host -NoNewline -Ba Black -Fo Cyan $Response
            if (!$line.Contains([char]0x03)) { Write-Host -NoNewline `n }
            else { Write-Host -Ba Black -Fo Gray (" (End Of Text)") }
            Write-Host -NoNewline -Ba Black -Fo White $Cursor
         }
         if ($line.Contains([char]0x03)) { break; }
     }
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

cls
Write-Host (".")`n
InitPort
#$FileName = Get-FileName(".\")


Write-Host -Fo DarkGray "Opening file          : " $FileName

$FileData = Get-Content -Path $FileName
try {
    Write-Host -NoNewline -Ba Black -Fo White ">"
# Loop until Ctrl-C press...
    while ($true) { 
        foreach ($line in $FileData) {
            $Parms = $line.Split(" ")
            If ($Parms[0].ToLower() -eq "pause") {
                $Period = [int]$Parms[1]                           # Convert to integer
                $tmp = $line.Substring($line.Length-2).ToLower()   # Last two characters of line
                Write-Host -NoNewline -Ba Black -Fo White $Cursor
                if ($tmp -eq "ms") { 
                    Write-Host -Ba Black -Fo Green " Pause" $Period "milli-seconds"
                    Start-sleep -Milliseconds $Period
                } 
                else { 
                    Write-Host -Ba Black -Fo Green  $(" " * $Margin) " Pause" $Period "seconds"
                    Start-sleep $Period 
                }
#                Write-Host -NoNewline -Ba Black -Fo White ">"
            }
            elseif ($Parms[0].ToLower() -eq "cls") { 
                cls
                Write-Host -NoNewline -Ba Black -Fo White ">"
            }
            elseif ($line[0] -eq "#") {
                Write-Host -Ba Black -Fo Green $line.substring(1)     # Remove first character
#                Write-Host -NoNewline -Ba Black -Fo White ">"
            }
            else { SndCmd($line + "`n") }
        }
    }
}
finally {
# Ctrl-C falls through here...
    Write-Host -Ba Black -Fo DarkGray "Ctrl-C or Timeout detected."
    Write-Host -Ba Black -Fo DarkGray "Closing Serial Port" $Port.portname
    $Port.Close()
}

