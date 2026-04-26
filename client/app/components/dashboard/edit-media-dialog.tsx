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
import { useUpdateMediaTitle } from '@/app/hooks/useUpdateMediaTitle';
import { Media } from '@/app/types/index';

type Props = {
  open: boolean;
  media: Media;
  onClose: () => void;
};

export default function EditMediaDialog({ open, media, onClose }: Props) {
  const [title, setTitle] = React.useState(media.title);
  const updateMutation = useUpdateMediaTitle();

  //Reset dialog when reopened
  React.useEffect(() => {
    if (open) {
      setTitle(media.title);
      updateMutation.reset();
    }
  }, [open, media.title]);

  const handleClose = () => {
    if (updateMutation.isPending) return; //only close if media has been updated
    onClose();
  };

  const trimmed = title.trim();
  const isUnchanged = trimmed === media.title;
  const canSubmit = !!trimmed && !isUnchanged && !updateMutation.isPending;

  const handleSubmit = () => {
    if (!canSubmit) return;
    updateMutation.mutate(
      { id: media.id, title: trimmed },
      { onSuccess: () => onClose() },
    );
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && canSubmit) handleSubmit();
  };

  return (
    <Dialog open={open} onClose={handleClose} fullWidth maxWidth="sm">
      <DialogTitle>Edit Media</DialogTitle>
      <DialogContent>
        <DialogContentText sx={{ mb: 2 }}>
          Update the Title for this item in the Library. Your original file will not be affected.
        </DialogContentText>
        <TextField
          autoFocus
          fullWidth
          required
          margin="dense"
          label="Title"
          value={title}
          onChange={(e) => setTitle(e.target.value)}
          onKeyDown={handleKeyDown}
          disabled={updateMutation.isPending}
        />
        {updateMutation.isError && (
          <Alert severity="error" sx={{ mt: 2, bgcolor: 'error.main', color: 'text.primary' }}>
            {updateMutation.error.message}
          </Alert>
        )}
      </DialogContent>
      <DialogActions>
        <Button onClick={handleClose} disabled={updateMutation.isPending}>
          Cancel
        </Button>
        <Button
          variant="contained"
          onClick={handleSubmit}
          disabled={!canSubmit}
          sx={{ bgcolor: 'secondary.main', color: 'primary.contrastText' }}
        >
          {updateMutation.isPending ? 'Saving…' : 'Save'}
        </Button>
      </DialogActions>
    </Dialog>
  );
}
