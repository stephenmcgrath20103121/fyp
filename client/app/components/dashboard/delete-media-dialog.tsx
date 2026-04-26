'use client'
import * as React from 'react';
import Dialog from '@mui/material/Dialog';
import DialogTitle from '@mui/material/DialogTitle';
import DialogContent from '@mui/material/DialogContent';
import DialogActions from '@mui/material/DialogActions';
import DialogContentText from '@mui/material/DialogContentText';
import Button from '@mui/material/Button';
import Alert from '@mui/material/Alert';
import { useDeleteMedia } from '@/app/hooks/useDeleteMedia';
import { Media } from '@/app/types/index';

type Props = {
  open: boolean;
  media: Media;
  onClose: () => void;
};

export default function DeleteMediaDialog({ open, media, onClose }: Props) {
  const deleteMutation = useDeleteMedia();

  //Reset dialog when reopened
  React.useEffect(() => {
    if (open) deleteMutation.reset();
  }, [open]);

  const handleClose = () => {
    if (deleteMutation.isPending) return; //only close if media has been deleted
    onClose();
  };

  const handleConfirm = () => {
    deleteMutation.mutate(media.id, { onSuccess: () => onClose() });
  };

  return (
    <Dialog open={open} onClose={handleClose} fullWidth maxWidth="xs">
      <DialogTitle>Delete media?</DialogTitle>
      <DialogContent>
        <DialogContentText>
          This will only remove <strong>{media.title}</strong> and all associated data from your Library. Your original file will not be affected.
        </DialogContentText>
        {deleteMutation.isError && (
          <Alert severity="error" sx={{ mt: 2, bgcolor: 'error.main', color: 'text.primary' }}>
            {deleteMutation.error.message}
          </Alert>
        )}
      </DialogContent>
      <DialogActions>
        <Button onClick={handleClose} disabled={deleteMutation.isPending}>
          Cancel
        </Button>
        <Button
          variant="contained"
          color="error"
          onClick={handleConfirm}
          disabled={deleteMutation.isPending}
        >
          {deleteMutation.isPending ? 'Deleting…' : 'Delete'}
        </Button>
      </DialogActions>
    </Dialog>
  );
}