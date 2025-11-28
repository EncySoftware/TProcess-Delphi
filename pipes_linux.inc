{
    This file is part of the Free Pascal run time library.
    Copyright (c) 1999-2000 by Michael Van Canneyt

    Linux specific part of pipe stream.

    See the file COPYING.FPC, included in this distribution,
    for details about the copyright.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

 **********************************************************************}

uses Posix.StrOpts, Posix.Unistd;

Function CreatePipeHandles (Var Inhandle,OutHandle : Integer; APipeBufferSize : Cardinal = 1024) : Boolean;
var
  pip : TPipeDescriptors;
begin
  Result := pipe(pip)=0;
  Inhandle := pip.ReadDes;
  OutHandle := pip.WriteDes;
end;

Function TInputPipeStream.GetNumBytesAvailable: DWord;
begin
  if ioctl(Handle, I_NREAD, @Result)<0 then
    Result := 0;
end;

function TInputPipeStream.GetPosition: Int64;
begin
  Result:=FPos;
end;

procedure TInputPipeStream.InvalidSeek;
begin
  Raise EPipeSeek.Create (ENoSeekMsg);
end;

procedure PipeClose (const FHandle: Integer); inline;
begin
  FileClose(FHandle);
end;
