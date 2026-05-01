'use client';
import * as React from 'react';
import Dialog from '@mui/material/Dialog';
import DialogTitle from '@mui/material/DialogTitle';
import DialogContent from '@mui/material/DialogContent';
import DialogActions from '@mui/material/DialogActions';
import Button from '@mui/material/Button';
import TextField from '@mui/material/TextField';
import LinearProgress from '@mui/material/LinearProgress';
import Typography from '@mui/material/Typography';
import Box from '@mui/material/Box';
import { useMutation, useQueryClient } from '@tanstack/react-query';
import { uploadMedia } from '@/app/api/mediaroot-server-api';

type Props = {
    open: boolean;
    onClose: () => void;
};

export default function UploadDialog({ open, onClose }: Props) {
    const [file, setFile]         = React.useState<File | null>(null);
    const [title, setTitle]       = React.useState('');
    const [progress, setProgress] = React.useState<number | null>(null);
    const queryClient             = useQueryClient();

    const mutation = useMutation({
        mutationFn: () => {
            if (!file) throw new Error('No file selected');
            setProgress(0);
            return uploadMedia(file, title || undefined, setProgress);
        },
        onSuccess: () => {
            queryClient.invalidateQueries({ queryKey: ['media'] });
            handleClose();
        },
    });

    const handleClose = () => {
        if (mutation.isPending) return;
        setFile(null);
        setTitle('');
        setProgress(null);
        mutation.reset();
        onClose();
    };

    const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
        const f = e.target.files?.[0] ?? null;
        setFile(f);
        //Use filename as title if unset by user
        if (f && !title) {
            setTitle(f.name.replace(/\.[^/.]+$/, ''));
        }
    };

    return (
        <Dialog open={open} onClose={handleClose} fullWidth maxWidth="sm">
            <DialogTitle>Add Media</DialogTitle>
            <DialogContent>
                <Box sx={{ display: 'flex', flexDirection: 'column', gap: 2, mt: 1 }}>
                    <Button
                        variant="outlined"
                        component="label"
                        disabled={mutation.isPending}
                        sx={{ textTransform: 'none' }}
                    >
                        {file ? file.name : 'Choose file…'}
                        <input
                            hidden
                            type="file"
                            accept="video/*,audio/*,image/*"
                            onChange={handleFileChange}
                        />
                    </Button>

                    <TextField
                        label="Title (optional)"
                        value={title}
                        onChange={(e) => setTitle(e.target.value)}
                        disabled={mutation.isPending}
                        fullWidth
                    />

                    {progress !== null && (
                        <Box>
                            <LinearProgress variant="determinate" value={progress} />
                            <Typography variant="caption">{progress}%</Typography>
                        </Box>
                    )}

                    {mutation.isError && (
                        <Typography color="error" variant="body2">
                            {(mutation.error as Error).message}
                        </Typography>
                    )}
                </Box>
            </DialogContent>
            <DialogActions>
                <Button onClick={handleClose} disabled={mutation.isPending}>
                    Cancel
                </Button>
                <Button
                    variant="contained"
                    disabled={!file || mutation.isPending}
                    onClick={() => { mutation.mutate(); handleClose();}}
                >
                    {mutation.isPending ? 'Uploading…' : 'Upload'}
                </Button>
            </DialogActions>
        </Dialog>
    );
}