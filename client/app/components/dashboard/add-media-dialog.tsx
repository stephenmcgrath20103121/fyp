'use client'
import * as React from 'react';
import Dialog from '@mui/material/Dialog';
import DialogTitle from '@mui/material/DialogTitle';
import DialogContent from '@mui/material/DialogContent';
import DialogActions from '@mui/material/DialogActions';
import DialogContentText from '@mui/material/DialogContentText';
import TextField from '@mui/material/TextField';
import Button from '@mui/material/Button';
import Alert from '@mui/material/Alert';
import { useCreateMedia } from '@/app/hooks/useCreateMedia';

type Props = {
  open: boolean;
  onClose: () => void;
};

export default function AddMediaDialog({ open, onClose }: Props) {
  const [filePath, setFilePath] = React.useState('');
  const [title, setTitle] = React.useState('');
  const createMedia = useCreateMedia();

  //Reset form when reopened
  React.useEffect(() => {
    if (open) {
      setFilePath('');
      setTitle('');
      createMedia.reset();
    }
  }, [open]);

  const handleClose = () => {
    if (createMedia.isPending) return; //only close if media has been added
    onClose();
  };

  const handleSubmit = () => {
    const trimmedPath = filePath.trim();
    if (!trimmedPath) return;
    const trimmedTitle = title.trim();
    createMedia.mutate(
      { filePath: trimmedPath, title: trimmedTitle || undefined },
      { onSuccess: () => onClose() },
    );
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && filePath.trim() && !createMedia.isPending) {
      handleSubmit();
    }
  };

  return (
    <Dialog open={open} onClose={handleClose} fullWidth maxWidth="sm">
      <DialogTitle>Add Media</DialogTitle>
      <DialogContent>
        <DialogContentText sx={{ mb: 2 }}>
          Enter the absolute path to a media file on the server.
        </DialogContentText>
        <TextField
          autoFocus
          fullWidth
          required
          margin="dense"
          label="File path"
          placeholder="/path/to/video.mp4"
          value={filePath}
          onChange={(e) => setFilePath(e.target.value)}
          onKeyDown={handleKeyDown}
          disabled={createMedia.isPending}
        />
        <TextField
          fullWidth
          margin="dense"
          label="Title (optional)"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          onKeyDown={handleKeyDown}
          disabled={createMedia.isPending}
        />
        {createMedia.isError && (
          <Alert severity="error" sx={{ mt: 2, bgcolor: 'error.main', color: 'text.primary' }}>
            {createMedia.error.message}
          </Alert>
        )}
      </DialogContent>
      <DialogActions>
        <Button onClick={handleClose} disabled={createMedia.isPending}>
          Cancel
        </Button>
        <Button
          variant="contained"
          onClick={handleSubmit}
          disabled={!filePath.trim() || createMedia.isPending}
          sx={{ bgcolor: 'secondary.main', color: 'primary.contrastText' }}
        >
          {createMedia.isPending ? 'Importing…' : 'Add'}
        </Button>
      </DialogActions>
    </Dialog>
  );
}
