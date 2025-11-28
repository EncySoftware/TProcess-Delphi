{%main ./process.pas}
{
    This file is part of the Free Component Library (FCL)
    Copyright (c) 1999-2008 by the Free Pascal development team

    See the file COPYING.FPC, included in this distribution,
    for details about the copyright.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 **********************************************************************}

{$DEFINE OS_HASEXITCODE}

uses
  System.AnsiStrings,
  Posix.Errno, Posix.Signal, Posix.Stdlib, Posix.SysWait, Posix.Unistd;

Resourcestring
  SNoCommandLine        = 'Cannot execute empty command-line';
  SErrNoSuchProgram     = 'Executable not found: "%s"';
  SErrNoTerminalProgram = 'Could not detect X-Terminal program';
  SErrCannotFork        = 'Failed to Fork process';

Const
  // unused as of yet, I assume these are nice levels ?
  PriorityConstants : Array [TProcessPriority] of Integer = (20,20,0,-20);

Const
  GeometryOption : String = '-geometry';
  TitleOption : String ='-title';


procedure TProcess.CloseProcessHandles;
begin
 // Do nothing. Win32 call.
end;

Function TProcess.GetExitCode : Integer;
begin
  if (Not Running) and wifexited(FExitCode) then
    Result:=wexitstatus(FExitCode)
  else
    Result:=0;
end;

Function TProcess.PeekExitStatus : Boolean;
var
  res: Integer;
begin
  repeat
    res:=waitpid(Handle,PInteger(@FExitCode),WNOHANG);
  until (res<>-1) or (errno<>EINTR);
  result:=res=Handle;
  If Not Result then
    FExitCode:=cardinal(-1); // was 0, better testable for abnormal exit.
end;

Type
  TPCharArray = Array[Word] of PAnsiChar;
  PPCharArray = ^TPcharArray;

Function StringsToPCharList(List : TStrings) : PPCharArray;
Var
  I : Integer;
  S : AnsiString;
begin
  I:=(List.Count)+1;
  GetMem(Result,I*sizeOf(PAnsiChar));
  Result[List.Count]:=Nil;
  For I:=0 to List.Count-1 do begin
    S:=List[i];
    Result[i] := AllocMem(Length(S) + 1);
    System.AnsiStrings.StrPCopy(Result[i], S);
    WriteLn('Arg ', I, ': <', Result[i], '>');
  end;
end;

Procedure FreePCharList(List : PPCharArray);
Var
  I : integer;
begin
  I:=0;
  While List[i]<>Nil do begin
    FreeMem(List[i]);
    Inc(I);
  end;
  FreeMem(List);
end;

Function MakeCommand(P : TProcess) : PPCharArray;
Var
  Cmd : String;
  S  : TStringList;
  G : String;
  PA,PC : String;
begin
  PA:=P.ApplicationName;
  PC:=P.CommandLine;
  If (PA='') and (PC='') and (P.Executable='') then
    Raise EProcess.Create(SNoCommandline);
  S:=TStringList.Create;
  try
    if (PA='') and (PC='') then
      begin
      S.Assign(P.Parameters);
      S.Insert(0,P.Executable);
      end
    else
      begin
      If (PA<>'') then
        Cmd:=PA
      else
        Cmd:=PC;
      CommandToList(Cmd,S);
      end;
    if poNewConsole in P.Options then
      begin
      S.Insert(0,'-e');
      If (PA<>'') then
        begin
        S.Insert(0,PA);
        S.Insert(0,'-title');
        end;
      if suoUseCountChars in P.StartupOptions then
        begin
        S.Insert(0,Format('%dx%d',[P.dwXCountChars,P.dwYCountChars]));
        S.Insert(0,'-geometry');
        end;
      If (P.XTermProgram<>'') then
        S.Insert(0,P.XTermProgram)
      { else }
        { S.Insert(0,DetectXterm); }
      end;
    if (PA<>'') then
      begin
      S.Add(TitleOption);
      S.Add(PA);
      end;
    G:='';
    if (suoUseSize in P.StartupOptions) then
      g:=format('%dx%d',[P.dwXSize,P.dwYsize]);
    if (suoUsePosition in P.StartupOptions) then
      g:=g+Format('+%d+%d',[P.dwX,P.dwY]);
    if G<>'' then
      begin
      S.Add(GeometryOption);
      S.Add(g);
      end;
    Result:=StringsToPcharList(S);
  Finally
    S.free;
  end;
end;

Type
  TPipeEnd = (peRead,peWrite);
  TPipePair = Array[TPipeEnd] of Integer;

Procedure CreatePipes(Var HI,HO,HE : TPipePair; CI, CE : Boolean);

  Procedure CreatePair(Var P : TPipePair);
   begin
    If not CreatePipeHandles(P[peRead],P[peWrite]) then
      Raise EProcess.Create('Failed to create pipes');
   end;

  Procedure ClosePair(Var P : TPipePair);
  begin
    if (P[peRead]<>-1) then
      FileClose(P[peRead]);
    if (P[peWrite]<>-1) then
      FileClose(P[peWrite]);
  end;

begin
  HO[peRead]:=-1;HO[peWrite]:=-1;
  HI[peRead]:=-1;HI[peWrite]:=-1;
  HE[peRead]:=-1;HE[peWrite]:=-1;
  Try
    CreatePair(HO);
    if CI then
      CreatePair(HI);
    If CE then
      CreatePair(HE);
  except
    ClosePair(HO);
    ClosePair(HI);
    If CE then
      ClosePair(HE);
    Raise;
  end;
end;

Function safedup2(fildes, fildes2 : Integer): Integer;
begin
  repeat
    Result:=dup2(fildes,fildes2);
  until (Result<>-1) or (errno<>EINTR);
end;


Function FileSearch (Const Name, DirList : AnsiString) : AnsiString;
Var
  I : longint;
  Temp : AnsiString;
begin
  Result:=Name;
  Temp:=DirList;
  for I:=1 to Length(Temp) do
    If CharInSet(Temp[I],['\', '/']) then
      Temp[i]:=PathDelim;

  while True do begin
    If Temp = '' then
      break; // No more directories to search - fail
    I:=Pos(PathSep,Temp);
    If I<>0 then
      begin
        Result:=Copy(Temp,1,i-1);
        system.Delete(Temp,1,I);
      end
    else
      begin
        Result:=Temp;
        Temp:='';
      end;
    If Result<>'' then
      begin
      if (Result<>'') then
        Result:=IncludeTrailingPathDelimiter(Result)+name;
      end;
    If (Result <> '') and FileExists(Result) Then
      exit;
  end;
  Result:='';
end;


Procedure TProcess.Execute;
Var
  HI,HO,HE : TPipePair;
  PID      : Longint;
  FEnv     : PPCharArray;
  Argv     : PPCharArray;
  fd       : Integer;
  res      : Integer;
  FoundName,
  PName    : AnsiString;
begin
  If (poUsePipes in FProcessOptions) then
    CreatePipes(HI,HO,HE,True,Not (poStdErrToOutPut in FProcessOptions));
  Try
    if FEnvironment.Count<>0 then
      FEnv:=StringsToPcharList(FEnvironment)
    else
      FEnv:=Nil;
    Try
      Argv:=MakeCommand(Self);
      Try
        If (Argv<>Nil) and (ArgV[0]<>Nil) then begin
          PName:=System.AnsiStrings.StrPas(Argv[0]);
        end else begin
          // This should never happen, actually.
          PName:=ApplicationName;
          If (PName='') then
            PName:=CommandLine;
        end;

        if not FileExists(PName) then begin
          FoundName := FileSearch(Pname,getenv('PATH'));
          if FoundName<>'' then
            PName:=FoundName
          else
            raise EProcess.CreateFmt(SErrNoSuchProgram,[PName]);
        end;

        Pid:=fork;
        if Pid<0 then
          Raise EProcess.Create(SErrCannotFork);
        if (PID>0) then
          begin
            // Parent process. Copy process information.
            FProcessHandle:=PID;
            FThreadHandle:=PID;
            FProcessId:=PID;
            //FThreadId:=PID;
          end
        else
          begin
            { We're in the child }
            if (FCurrentDirectory<>'') then
               ChDir(FCurrentDirectory);
            if PoUsePipes in Options then
              begin
                FileClose(HI[peWrite]);
                safedup2(HI[peRead],0);
                FileClose(HO[peRead]);
                safedup2(HO[peWrite],1);
                if (poStdErrToOutPut in Options) then
                  safedup2(HO[peWrite],2)
                else
                  begin
                    FileClose(HE[peRead]);
                    safedup2(HE[peWrite],2);
                  end;
              end
            else if poNoConsole in Options then
              begin
                fd:=FileOpen('/dev/null',fmOpenReadWrite or fmShareDenyNone);
                safedup2(fd,0);
                safedup2(fd,1);
                safedup2(fd,2);
              end;
            if Assigned(FForkEvent) then
              FForkEvent(Self);
            if (poRunSuspended in Options) then
              __raise(SIGSTOP);
            if FEnv<>Nil then
              execve(PAnsiChar(PName),PPAnsiChar(Argv),PPAnsiChar(Fenv))
            else
              execv(PAnsiChar(PName),PPAnsiChar(Argv));
            __exit(127);
          end
      Finally
        FreePcharList(Argv);
      end;
    Finally
      If (FEnv<>Nil) then
        FreePCharList(FEnv);
    end;
  Finally
    if POUsePipes in FProcessOptions then
      begin
        FileClose(HO[peWrite]);
        FileClose(HI[peRead]);
        if Not (poStdErrToOutPut in FProcessOptions) then
          FileClose(HE[peWrite]);
        CreateStreams(HI[peWrite],HO[peRead],HE[peRead]);
      end;
  end;
  FRunning:=True;
  if not (csDesigning in ComponentState) and // This would hang the IDE !
     (poWaitOnExit in FProcessOptions) and
      not (poRunSuspended in FProcessOptions) then
    WaitOnExit;
end;

Function TProcess.WaitOnExit : Boolean;
begin
  if FRunning then
    wait(@FExitCode);
  Result:=(FExitCode>=0);
  FRunning:=False;
end;

Function TProcess.Suspend : Integer;
begin
  Result := kill(Handle,SIGSTOP);
end;

Function TProcess.Resume : Integer;
begin
  Result := kill(Handle,SIGSTOP);
end;

Function TProcess.Terminate(AExitCode : Integer; SignalWaitTime: Integer) : Boolean;
begin
  if aExitCode<>0 then ; // silence compiler warning
  Result:=kill(Handle,SIGTERM)=0;
  If Result then
    begin
    // Give the process some time to handle it. Sleeping may also yield to the process.
    if SignalWaitTime>0 then
      Sleep(SignalWaitTime);
    // Not handled yet ?
    if Running then
      Result:=kill(Handle,SIGKILL)=0;
    end;
  { the fact that the signal has been sent does not
    mean that the process has already handled the
    signal -> wait instead of calling getexitstatus }
  if Result then
    WaitOnExit;
end;

Procedure TProcess.SetShowWindow (Value : TShowWindowOptions);
begin
  FShowWindow:=Value;
end;
