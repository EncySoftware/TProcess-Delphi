program posix_spawn_threads;

{$APPTYPE CONSOLE}

uses
  System.Classes,
  System.SysUtils,
  Posix.Dlfcn,
  Posix.Unistd,
  dprocess;

type
  TWorker = class(TThread)
  private
    FExitCode: Integer;
    FError: string;
  protected
    procedure Execute; override;
  public
    property ExitCode: Integer read FExitCode;
    property ErrorText: string read FError;
  end;

procedure TWorker.Execute;
var
  P: TProcess;
begin
  try
    P := TProcess.Create(nil);
    try
      P.Executable := '/bin/sh';
      P.Parameters.Add('-c');
      P.Parameters.Add('if [ "$TPROCESS_ENV_TEST" = "inherited-value-42" ]; then exit 17; else exit 19; fi');
      P.Execute;
      P.WaitOnExit;
      FExitCode := P.ExitCode;
    finally
      P.Free;
    end;
  except
    on E: Exception do
      FError := E.ClassName + ': ' + E.Message;
  end;
end;

var
  Workers: array[0..15] of TWorker;
  I: Integer;
  Env: PMarshaledAString;

begin
  Env := Posix.Unistd.environ();
  if (Env = nil) or (Env^ = nil) then
    raise Exception.Create('Posix.Unistd.environ returned no environment');
  for I := Low(Workers) to High(Workers) do
  begin
    Workers[I] := TWorker.Create(True);
    Workers[I].Start;
  end;
  for I := Low(Workers) to High(Workers) do
  begin
    Workers[I].WaitFor;
    if Workers[I].ErrorText <> '' then
      raise Exception.CreateFmt('worker %d failed: %s', [I, Workers[I].ErrorText]);
    if Workers[I].ExitCode <> 17 then
      raise Exception.CreateFmt('worker %d returned %d, expected 17', [I, Workers[I].ExitCode]);
    Workers[I].Free;
  end;
  Writeln('PASS: ', Length(Workers), ' concurrent TProcess launches');
end.
